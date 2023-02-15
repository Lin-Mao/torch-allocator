#include "python_states.h"

namespace c10 {
namespace cuda {
namespace AllocatorSim {

PythonStateMonitor& PythonStateMonitor::instance() {
    static PythonStateMonitor monitor;
    return monitor;
}

// Take from PyTorch::THPUtils_unpackStringView
std::string PythonStateMonitor::unpack_pyobject(PyObject* obj) {
    if (PyBytes_Check(obj)) {
        size_t size = PyBytes_GET_SIZE(obj);
        return std::string(PyBytes_AS_STRING(obj), size);
    }
    if (PyUnicode_Check(obj)) {
        // NOLINTNEXTLINE(cppcoreguidelines-init-variables)
        Py_ssize_t size;
        const char* data = PyUnicode_AsUTF8AndSize(obj, &size);
        if (!data) {
            // If we get any runtime error, just return an empty string to continue running
            // printf("obj %p utf8 parsing error", obj);
            return "";
        }
        return std::string(data, (size_t)size);
    }
    // printf("obj %p not bytes or unicode", obj);
    return "";
}

std::vector<PythonState>& PythonStateMonitor::get_states(bool cached) {
    if (cached) {
        return _states;
    }

    // GIL lock is required
    pybind11::gil_scoped_acquire gil;

    PyFrameObject* frame = PyEval_GetFrame();
    _states.clear();

    while (nullptr != frame) {
        size_t lineno = PyFrame_GetLineNumber(frame);
        size_t co_firstlineno = frame->f_code->co_firstlineno;
        std::string file_name = unpack_pyobject(frame->f_code->co_filename);
        std::string function_name = unpack_pyobject(frame->f_code->co_name);
        _states.emplace_back(PythonState{file_name, function_name, co_firstlineno, lineno});
        frame = frame->f_back;
    }
    return _states;
}

bool python_state_get(size_t max_num_states, python_state_t *states, size_t *num_states) {

    bool status = true;

    auto &python_state_monitor = PythonStateMonitor::instance();

    auto &python_states = python_state_monitor.get_states();

    if (python_states.empty()) {
        status = false;
    } else {
        status = true;

        *num_states = std::min(python_states.size(), max_num_states);
        for (size_t i = 0; i < *num_states; ++i) {
            states[i].file_name = python_states[i].file_name.c_str();
            states[i].function_name = python_states[i].function_name.c_str();
            states[i].function_first_lineno = python_states[i].function_first_lineno;
            states[i].lineno = python_states[i].lineno;
        }
    }

  return status;
}

}  // namespace c10
}  // namespace cuda
}  // namespace AllocatorSim