#pragma once
#include <string>
#include <functional>

// #include <cstdio>
// #include <memory>
// #include <filesystem>
// #include <pybind11/numpy.h>


// struct Action {
//     void setA(const std::string &a_) { a = a_; }
//     void setB(const py::array_t<float> &b_) { b = b_; }

//     const std::string &getA() const { return a; }
//     const py::array_t<float> &getB() const { return b; }

//     std::string a;
//     py::array_t<float> b;
// };

// struct State {
//     void setA(const std::string &a_) { a = a_; }
//     void setB(const py::array_t<float> &b_) { b = b_; }

//     const std::string &getA() const { return a; }
//     const py::array_t<float> &getB() const { return b; }

//     std::string a;
//     py::array_t<float> b;
// };

// using CppCB = std::function<void(Action)>;
// using PyCB = std::function<void(State)>;
// using PyCBInit = std::function<void(CppCB)>;

// NOTE: use this to call from connector
// extern "C" int __attribute__((visibility("default"))) mymain(
//   std::string resourcedir,
//   bool headless,
//   const std::function<void(int)> &callback);

// NOTE: use this to allow breakpoints in debugger...
int mymain(std::string resdir, std::string mapname, bool ai = false);

[[noreturn]] void handleFatalError(const std::string & message, bool terminate);
