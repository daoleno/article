# 第一步：GCC 的编译过程

---

这一章将详细描述 GCC 是如何将源文件翻译为可执行文件的。编译过程有多个阶段，包括 GNU 编译器（`gcc` 或 `g++` 前端），GNU 汇编器 `as` 和 GNU 链接器 `ld`。

## 编译过程概览

单个 GCC 调用执行的命令序列包括以下阶段

* 预处理（展开宏）
* 编译（由源代码生成汇编语言）
* 汇编（由汇编语言生成机器码）
* 链接（创建可执行文件）

作为示例，我们将利用 `hello.c` 程序单独执行这些编译阶段

```cpp
#include <stdio.h>

int main(void) {
  printf("Hello, world!\n");
  return 0;
}
```

注意：不必使用本节中描述的任何单个命令来编译程序。所有命令都由 GCC 内部自动执行，并且可以使用 `-v` 选项来查看执行过程。本章的目的是为了了解编译器的工作原理。

尽管 Hello World 程序很简单，但它使用了外部头文件和库，因此执行了编译过程的所有主要步骤。

## 预处理器（preprocessing）

编译过程的第一个阶段是使用预处理器来展开宏并且将头文件包含进来。要执行此阶段，GCC 执行以下命令

```zsh
cpp hello.c > hello.i
```

生成文件为 `hello.i`, 其中包含展开了所有宏的源代码。按照惯例，预处理文件的 C 程序文件拓展名为 `.i`， C++ 程序的文件拓展名为 `.ii`。实际上，除非使用 `-save-temps` 选项，否则预处理文件不会保存到磁盘。

## 编译（compilation）

编译过程的下一个阶段，是将预处理的源代码编译为特定处理器的汇编语言。命令行选项 `-S` 指示 gcc 将预处理的 C 源代码转换为汇编语言，而不创建目标文件。

```zsh
gcc -Wall -S hello.i
```

生成的汇编语言存储在 `hello.s` 文件中。下面是 Hello World 在 Intel x86-64 处理器上的汇编结果

```zsh
➜  demo cat hello.s
        .file   "hello.c"
        .text
        .section        .rodata
.LC0:
        .string "Hello, world!"
        .text
        .globl  main
        .type   main, @function
main:
.LFB0:
        .cfi_startproc
        pushq   %rbp
        .cfi_def_cfa_offset 16
        .cfi_offset 6, -16
        movq    %rsp, %rbp
        .cfi_def_cfa_register 6
        movl    $.LC0, %edi
        call    puts
        movl    $0, %eax
        popq    %rbp
        .cfi_def_cfa 7, 8
        ret
        .cfi_endproc
.LFE0:
        .size   main, .-main
        .ident  "GCC: (GNU) 8.1.1 20180712 (Red Hat 8.1.1-5)"
        .section        .note.GNU-stack,"",@progbits
```

注意：调用外部函数时，由于 `printf` 中不需要格式化字符串，并且有 `\n`, 所以 gcc 将其优化成了对于 `puts` 的调用。可以理解为将 `printf("Hello, world!\n")` 优化为 `puts("Hello, world!")`

## 汇编（assembly）

汇编器的目的是将汇编语言转换为机器代码并生成目标文件。当在汇编文件中调用外部函数时，汇编器会保留未定义的外部函数地址，以便稍后由链接器填充。可以使用以下命令调用汇编程序

```zsh
as hello.s -o hello.o
```

与 GCC 一样，输出文件使用 `-o` 选项指定。生成的文件 `hello.o` 包含 Hello World 程序的机器指令，其中包含对 `puts` 未定义的引用。

## 链接（linking）

编译的最后阶段是链接目标文件以创建可执行文件。实际上可执行文件需要许多来自系统和 C 运行时(C run-time)库的外部函数。因为，GCC 内部使用的实际链接命令很复杂。例如，链接 Hello World 程序的完整命令是：

```
ld -dynamic-linker /lib64/ld-linux-x86-64.so.2 /usr/lib/gcc/x86_64-redhat-linux/8/../../../../lib64/crt1.o /usr/lib/gcc/x86_64-redhat-linux/8/../../../../lib64/crti.o /usr/lib/gcc/x86_64-redhat-linux/8/crtbegin.o -L/usr/lib/gcc/x86_64-redhat-linux/8 -L/usr/lib/gcc/x86_64-redhat-linux/8/../../../../lib64 -L/lib/../lib64 -L/usr/lib/../lib64 -L/usr/lib/gcc/x86_64-redhat-linux/8/../../.. hello.o -lgcc --as-needed -lgcc_s --no-as-needed -lc -lgcc --as-needed -lgcc_s --no-as-needed /usr/lib/gcc/x86_64-redhat-linux/8/crtend.o /usr/lib/gcc/x86_64-redhat-linux/8/../../../../lib64/crtn.o
```

幸运的是，永远不需要直接输入上面的命令。像下面这样调用时， gcc 将自动处理整个链接过程

```zsh
gcc hello.o
```
这会将目标文件 `hello.o` 链接到 C 标准库，并生成一个可执行文件 `a.out`

```zsh
➜  demo ./a.out
Hello, world!
```

可以使用 `g++` 命令以相同的方式将 C++ 程序的目标文件链接到 C++ 标准库。