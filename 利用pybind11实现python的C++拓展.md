# 利用 pybind11 实现 python 的 C++ 拓展

## 安装 pybind11
```
pip3 install pybind11
```

## C++ 示例
示例中包含了一个简单的打印字符串的类
```cpp

#include <iostream>
#include <pybind11/pybind11.h>

using namespace std;

class Print {
public:
  string name;
  void printName() { cout << "name is: " << name; }
};

int call_printName() {
  Print obj;
  obj.name = "pybind11";
  obj.printName();
  return 0;
}

namespace py = pybind11;

PYBIND11_MODULE(python_example, m) {
  py::class_<Print> print(m, 'Print');
  print.def("printName", &Print::printName);
  m.def("call_printName", &call_printName);
}
```
`PYBIND11_MODULE` 宏 创了建一个函数，在 python 程序中导入时将被调用。模块名称（`python_example`）是作为第一个宏参数给出的（不应该用引号括起来）。第二个参数（`m`）定义了一个类型为 py::module 的变量，它是创建绑定的主要接口。

pybind11 利用 C++ 的模板技术实现 C++ 的类与 Python 接口的绑定。
方法 module::def() 生成 printName() 函数，将其暴露给 python。

## 构建
有多种方式实现 pybind11 的构建，这里利用 setuptoos 构建模块

创建 setup.py, 代码来自 [python_example](https://github.com/pybind/python_example/blob/master/setup.py)
```python
from setuptools import setup, Extension
from setuptools.command.build_ext import build_ext
import sys
import setuptools

__version__ = '0.0.1'


class get_pybind_include(object):
    """Helper class to determine the pybind11 include path

    The purpose of this class is to postpone importing pybind11
    until it is actually installed, so that the ``get_include()``
    method can be invoked. """

    def __init__(self, user=False):
        self.user = user

    def __str__(self):
        import pybind11
        return pybind11.get_include(self.user)


ext_modules = [
    Extension(
        'python_example',
        ['src/main.cpp'],
        include_dirs=[
            # Path to pybind11 headers
            get_pybind_include(),
            get_pybind_include(user=True)
        ],
        language='c++'
    ),
]


# As of Python 3.6, CCompiler has a `has_flag` method.
# cf http://bugs.python.org/issue26689
def has_flag(compiler, flagname):
    """Return a boolean indicating whether a flag name is supported on
    the specified compiler.
    """
    import tempfile
    with tempfile.NamedTemporaryFile('w', suffix='.cpp') as f:
        f.write('int main (int argc, char **argv) { return 0; }')
        try:
            compiler.compile([f.name], extra_postargs=[flagname])
        except setuptools.distutils.errors.CompileError:
            return False
    return True


def cpp_flag(compiler):
    """Return the -std=c++[11/14] compiler flag.

    The c++14 is prefered over c++11 (when it is available).
    """
    if has_flag(compiler, '-std=c++14'):
        return '-std=c++14'
    elif has_flag(compiler, '-std=c++11'):
        return '-std=c++11'
    else:
        raise RuntimeError('Unsupported compiler -- at least C++11 support '
                           'is needed!')


class BuildExt(build_ext):
    """A custom build extension for adding compiler-specific options."""
    c_opts = {
        'msvc': ['/EHsc'],
        'unix': [],
    }

    if sys.platform == 'darwin':
        c_opts['unix'] += ['-stdlib=libc++', '-mmacosx-version-min=10.7']

    def build_extensions(self):
        ct = self.compiler.compiler_type
        opts = self.c_opts.get(ct, [])
        if ct == 'unix':
            opts.append('-DVERSION_INFO="%s"' % self.distribution.get_version())
            opts.append(cpp_flag(self.compiler))
            if has_flag(self.compiler, '-fvisibility=hidden'):
                opts.append('-fvisibility=hidden')
        elif ct == 'msvc':
            opts.append('/DVERSION_INFO=\\"%s\\"' % self.distribution.get_version())
        for ext in self.extensions:
            ext.extra_compile_args = opts
        build_ext.build_extensions(self)

setup(
    name='python_example',
    version=__version__,
    author='Sylvain Corlay',
    author_email='sylvain.corlay@gmail.com',
    url='https://github.com/pybind/python_example',
    description='A test project using pybind11',
    long_description='',
    ext_modules=ext_modules,
    install_requires=['pybind11>=2.2'],
    cmdclass={'build_ext': BuildExt},
    zip_safe=False,
)
```
## 安装

代码目录如下

```
.
├── setup.py
└── src
    ├── main.cpp
    └── test.py
```

```
pip3 install .
```

## python 调用 C++ 接口

test.py 
```python
import python_example
python_example.call_printName()
```

执行结果
```
name is: pybind11
```

## 相关链接

[pybind11](https://github.com/pybind/pybind11)

[python_example](https://github.com/pybind/python_example)

[Integrate Python And C++](https://stackoverflow.com/questions/1153577/integrate-python-and-c)

[CppCon 2016: “Introduction to C++ python extensions and embedding Python in C++ Apps"](https://www.youtube.com/watch?v=bJq1n4gQFfw&t=10s)
