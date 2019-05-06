# Golang: 动手实现自己的容器

## 容器简介

2013 年 3 月 Docker 的开源版本引发了软件行业对于打包和部署现代应用程序方式的观念的重大转变。随着 Docker 的出现，许多竞争、互补的技术也随之而来，这也导致了围绕这种技术的大肆宣传，增添了些许神秘感。本课程旨在揭开这些神秘面纱，带你了解容器技术的内部原理。

为了真正理解软件世界中的容器是什么，我们就需要理解编写一个容器的过程中发生了什么。在本教程中，我们首先会讨论什么是容器及容器化，linux 容器的基本概念（包括 namespaces, cgroups 和 layered filesystems)，然后我们会编写一些代码来从零完成一个简单的容器，放心，代码不会超过 100 行。

### 容器究竟是什么？

首先根据你的认知，假设一下，什么是`容器`？

想好了吗？让我来猜猜你将会说什么：

你也许会提到下面这些：

* 一种共享资源的方式
* 进程隔离
* 有点像轻量级的虚拟化
* 将根文件系统和元数据打包在一起
* 有点像 chroot 监狱
* 行为类似于 docker

对于一个词来说这些概念已经很多了，`容器` 已经被用于很多场合（有些是重叠的）。它被用来指代容器化，和实现容器化的技术。

### 故事开始之前

一开始的时候，有这么一个程序。让我们叫它 `run.sh`, 我们会将它拷贝的远程服务器上，然后运行它。然而，在远程服务器上执行任意代码是不安全的，并且很难管理和拓展。所以我们发明了虚拟专用服务器和用户权限。它们的效果还可以。

但是 `run.sh` 有依赖。它需要主机上有特定的库。并且它在远程和本地的执行效果不一定完全相同。所以我们发明了 AMIs (Amazon Machine Images)，VMDKs (VMware images)， Vagrantfiles 和一系列类似的东西。它们的效果也还可以。

嗯，它们都能用。但是软件集很大，我们很难有效的部署这些软件，因为它们都不怎么标准化。所以，我们又发明了缓存。

OK，现在的状况更好一些了。

缓存是让 docker 镜像比 vmdks 和 vagrantfiles 更有效的主要原因。它允许我们将增量传输到一些常见的基本镜像上，而不是移动整个镜像。这意味这我们可以将整个环境从一个地方迁移到另一个地方。这就是为什么当你执行 ``docker run whatever` 的时候，它差不多是立即执行的，即使这个 whatever 是一个操作系统镜像。

以上就是容器的意义所在。它就是关于打包、约束依赖，以帮助我们可重复的、安全的部署代码。但这是一个抽象概念，不是定义。所以接下来我们就聊聊实现细节。

## 第一步：准备构建一个容器

所以（这一次）什么是容器？如果创建一个容器就像执行一个创建容器的系统调用，那生活就轻松多了。虽然真实情况不是这样，但是老实说，很接近了。

了解容器的底层，我们必须得知道三个概念。分别是 `namespaces`, `cgroups` 和 `layered filesystems`。虽然还有其他的东西，但是这三个是容器魔法的主要器具。

### namespaces

[Linux Namespace](http://man7.org/linux/man-pages/man7/namespaces.7.html) 是 Linux 提供的一种内核级别环境隔离的方法。它提供了在一台计算机上运行多个容器所需的隔离，同时为每个容器提供了看起来像它自己独有的环境。目前有 6 种命名空间。每个都可以被独立的请求，相当于给进程（及其子进程）一个视图，可以使用其机器资源的子集。

命名空间有：

* `PID`：pid 命名空间给予一个进程及其子进程它们自己的视图，这个视图可以看到系统进程中与其相关的进程。可以把它看作一个映射表，当 pid 命名空间中的进程向内核请求进程列表时，内核会在映射表中查找。如果表中存在该进程，则使用映射的 ID 而不是实际 ID。如果映射表中不存在，则内核假装它根本不存在。pid 命名空间使得在其中创建的第一个进程 pid 为 1（通过将其主机 ID 映射到1），并在容器中提供隔离的进程树的展示。

* `MNT`：在某种程度上，这一点是最重要的。 mount 命名空间为其中包含的进程提供了自己的挂载表。这意味着他们可以在不影响其他命名空间（包括主机命名空间）的情况下挂载和卸载目录。更重要的是，结合 `chroot` 系统调用，它允许进程拥有自己的文件系统。通过切换容器看到的文件系统，我们可以让一个进程认为它在 ubuntu，busybox 或 alpine 上运行。

* `NET`：网络命名空间为使用它的进程提供了自己的网络堆栈。通常，只有主网络命名空间（启动计算机时启动的进程）才会实际连接任何真实的物理网卡。但我们可以创建虚拟以太网对 - 链接的以太网卡，其中一端可以放在一个网络命名空间中，另一端可以在网络命名空间之间创建虚拟链接。有点像在一台主机上有多个 ip 堆栈互相交谈。通过一些路由魔术，允许每个容器与现实世界通信，同时将每个容器隔离到其自己的网络堆栈中。

* `UTS`：UTS 命名空间为其进程提供了自己的系统主机名和域名视图。进入 UTS 命名空间后，设置主机名或域名不会影响其他进程。

* `IPC`：IPC 命名空间隔离了各种进程间通信机制，例如消息队列。

* `USER`：用户命名空间是最近添加的，从安全角度来看可能是最强大的。用户命名空间将进程看到的 uid 映射到主机上的另一组 uids（和gids）。这非常有用。使用用户命名空间，我们可以将容器的 root 用户 ID（即 0 ）映射到主机上的任意（非特权）uid。
这意味着我们可以让容器认为它具有 root 访问权限 - 我们甚至可以在不必实际赋予它在根命名空间中的任何权限的情况下在容器特定的资源上赋予它类似 root 的权限。容器可以自由的使用 uid 0 去运行进程 - 这通常与拥有 root 权限同义 - 但是内核实际上是将这个 uid 映射到一个无特权的真实 uid 上。大多数容器系统都没有将任何容器中的 uid 映射到所调用命名空间中的 uid 0：换句话说，容器中根本没有具有真正 root 权限的 uid。

大多数容器技术将用户的进程放入所有上述命名空间中，并初始化命名空间以提供标准环境。比如对于网络来说，这相当于在容器的隔离网络命名空间中创建初始网卡，并连接到主机真实网络上。


### CGroups

[Linux CGroup](http://man7.org/linux/man-pages/man7/cgroups.7.html) 全称 Linux Control Group， 是Linux内核的一个功能，用来限制，控制与分离一个进程组群的资源（如CPU、内存、磁盘输入输出等）。

从根本上说，cgroup 收集一组进程或任务 ID，并对它们加以限制。在 namespace 隔离进程的地方，cgroup 加强了在进程之间共享资源的公平性（或不公平性 - 这取决于你）。

内核将 cgroup 暴露为一个你可以挂载的特殊文件系统。只需将进程 ID 添加到任务文件里，即可通过编辑该目录中的文件来读取和配置各种值，从而将进程或线程添加到cgroup。

### Layered Filesystems

Namespaces 和 CGroup 是容器化中关于隔离和资源共享方面的技术。它们是码头上面的金属护盾和安全守卫。分层文件系统指示了我们如何有效的移动整个机器镜像，它们是船为何可以漂浮而不是下沉的原因。

最基本的，分层文件系统相当于优化调用，用来给每个容器创建根文件系统的副本。有很多方法可以做到这一点。[Btrfs](https://btrfs.wiki.kernel.org/index.php/Main_Page) 在文件系统层使用 `copy on write`。 [Aufs](https://en.wikipedia.org/wiki/Aufs) 使用 `union mounts`。由于有很多方法可以实现这一步骤，因此本文将使用非常简单的方式：我们直接做一个副本。虽然它很慢，但它确实有效。

## 第二步：构建容器 - 建立骨架

让我们先编写好粗糙的项目骨架，假设你已经安装了最新版本的 [golang](https://golang.org/doc/install), 就可以打开编辑器，把下面的代码拷贝过去。

PS: 接下来的程序需要你通过 `root` 权限执行

main.go - version 1

```go
package main

import (
	"fmt"
	"os"
	"os/exec"
	"syscall"
)

// go run main.go run <cmd> <args>
func main() {
	switch os.Args[1] {
	case "run":
		parent()
	case "child":
		child()
	default:
		panic("wat should I do")
	}
}

func parent() {
	fmt.Printf("Running %v \n", os.Args[2:])

	cmd := exec.Command("/proc/self/exe", append([]string{"child"}, os.Args[2:]...)...)
	cmd.Stdin = os.Stdin
	cmd.Stdout = os.Stdout
	cmd.Stderr = os.Stderr

	must(cmd.Run())
}

func child() {

	cmd := exec.Command(os.Args[2], os.Args[3:]...)
	cmd.Stdin = os.Stdin
	cmd.Stdout = os.Stdout
	cmd.Stderr = os.Stderr

	must(cmd.Run())
}

func must(err error) {
	if err != nil {
		panic(err)
	}
}
```

上面的代码做了什么呢？首先我们运行 `main.go`， 并且读取第一个参数。如果参数为 `run`, 那么我们将会运行 `parent()` 方法，如果参数为 `child`，运行 `child` 方法。可以看到，`parent` 方法运行了 `/proc/self/exe`，它是一个包含当前可执行文件的内存映像的特殊文件，换句话说，我们运行自己本身，但是将 `child` 作为第一个参数。

你也许会问，这有什么意义呢？好吧，目前还没什么用。它仅仅让我们执行了一个用户请求的程序（由 `os.Args[2:]` 提供），但是，通过这个简单的脚手架，我们可以创建一个容器。

来实际运行这看看我们可以做什么吧？

```
➜  test git:(master) ✗ go run main.go run echo "hello world"
Running [echo hello world] 
hello world
```

可以看到，我们目前可以正常的执行一条 linux 指令 `echo`。

那让我们再探究一点系统相关的东西。

```
➜  test git:(master) ✗ go run main.go run /bin/bash 
Running [/bin/bash] 
[root@daodao test]# ps
  PID TTY          TIME CMD
16019 pts/2    00:00:00 sudo
16026 pts/2    00:00:00 su
16028 pts/2    00:00:01 zsh
20999 pts/2    00:00:00 go
21052 pts/2    00:00:00 main
21057 pts/2    00:00:00 exe
21062 pts/2    00:00:00 bash
21103 pts/2    00:00:00 ps
```

我们运行了一个 bash, 利用 `ps` 指令来查看当前进程列表，发现显示的是宿主机的进程（PID 都很高），这是因为当前程序并没有运行在一个隔离的环境中，下一步我们就为他加上命名空间的隔离限制。

## 第三步：构建容器 - 添加 namespaces

要为我们的程序添加一些命名空间，我们只需要在 `parent()` 方法下添加一行代码，这行代码告诉 go 在运行子进程时传递一些额外的标志。

```go
cmd.SysProcAttr = &syscall.SysProcAttr{
	Cloneflags: syscall.CLONE_NEWUTS | syscall.CLONE_NEWPID | syscall.CLONE_NEWNS,
}
```

如果你现在运行程序，那么程序将会运行在 UTS, PID 和 MNT 命名空间里。

main.go - version 2

```go
package main

import (
	"fmt"
	"os"
	"os/exec"
	"syscall"
)

// go run main.go run <cmd> <args>
func main() {
	switch os.Args[1] {
	case "run":
		parent()
	case "child":
		child()
	default:
		panic("wat should I do")
	}
}

func parent() {
	fmt.Printf("Running %v \n", os.Args[2:])

	cmd := exec.Command("/proc/self/exe", append([]string{"child"}, os.Args[2:]...)...)
	cmd.SysProcAttr = &syscall.SysProcAttr{
		Cloneflags: syscall.CLONE_NEWUTS | syscall.CLONE_NEWPID | syscall.CLONE_NEWNS,
	}
	cmd.Stdin = os.Stdin
	cmd.Stdout = os.Stdout
	cmd.Stderr = os.Stderr

	must(cmd.Run())
}

func child() {

	cmd := exec.Command(os.Args[2], os.Args[3:]...)
	cmd.Stdin = os.Stdin
	cmd.Stdout = os.Stdout
	cmd.Stderr = os.Stderr

	must(cmd.Run())
}

func must(err error) {
	if err != nil {
		panic(err)
	}
}
```

既然运行在命名空间里了，是不是这一次执行 `ps`， 就会看到容器独有的进程列表了呢？

```
➜  test git:(master) ✗ go run main.go run /bin/bash
Running [/bin/bash] 
[root@daodao test]# ps
  PID TTY          TIME CMD
16019 pts/2    00:00:00 sudo
16026 pts/2    00:00:00 su
16028 pts/2    00:00:01 zsh
21993 pts/2    00:00:00 go
22045 pts/2    00:00:00 main
22050 pts/2    00:00:00 exe
22055 pts/2    00:00:00 bash
22090 pts/2    00:00:00 autojump <defunct>
22094 pts/2    00:00:00 ps
```

再运行一边程序，发现并没有。这就涉及到了 `ps` 指令的具体实现细节。 `ps` 并不是直接获取进程信息的，而是从 `/proc` 下获取的。

执行以下指令可以看到 `/proc` 下的程序是对实际程序的软链接。`/proc` 非常特殊，因为它是一个虚拟文件系统。有时候被称为进程信息伪文件系统。它并不包含“真实”文件，而是包含运行时的系统信息（例如系统内存，安装的设备，硬件配置等）。

```
➜  test git:(master) ✗ ls -l /proc/self/exe 
lrwxrwxrwx. 1 root root 0 10月 18 17:30 /proc/self/exe -> /usr/bin/ls
```

所以我们下一步需要将 `proc` 挂载进来，以查看进程信息。

## 第四步：构建容器 - 添加根文件系统

当前你的进程位于一组独立的命名空间中（你也可以尝试将其他命名空间添加到上一步的 `Cloneflags` 中）。但文件系统看起来与主机相同。这是因为你虽然在 `mount namespace` 中，但初始挂载是从根命名空间继承来的。

让我们改变这一点。我们需要以下两行简单的代码来切换到一个根文件系统。请将代码放在 `child()` 函数里。

```go
must(syscall.Chroot("rootfs"))
must(os.Chdir("/"))
```

这两行很重要，它们告诉操作系统将位于 `rootfs` 作为根文件系统。调用完成后，容器中的 `/` 目录将引用 `rootfs`。`rootfs` 是我从 `ubuntu:bionic` 镜像上拷贝出来的，放置于 demo 目录下, 使用 `tar -xvf rootfs.tar.gz` 进行解压。你也可以使用自己生成的文件系统。

运行我们的程序，我们已经有了自己的文件系统

```
➜  test git:(master) ✗ go run main.go run /bin/bash
Running [/bin/bash] 
root@daodao:/# ls
bin  boot  dev  etc  home  lib  lib64  media  mnt  opt  proc  root  run  sbin  srv  sys  tmp  usr  var
```

```
root@daodao:/# ps
Error, do this: mount -t proc proc /proc
```

但是我们执行 `ps` 指令的时候，提示我们要挂载 `proc`。原因如上节所说， `ps` 指令实际上查看的是 `/proc` 下的进程，而 `/proc` 是一个伪文件系统。需要挂载进来。

我们继续在代码中添加如下两行

```go
must(syscall.Mount("proc", "proc", "proc", 0, ""))
# .... some code ...
must(syscall.Unmount("proc", 0))
```

bingo! 运行程序后，可以看到 `ps` 成功执行，而且进程号也是从 1 开始，意味着我们的进程运行在前面所说的 `PID namespace`中。

```
➜  demo git:(master) ✗ go run main.go run /bin/bash
root@daodao:/# ps
  PID TTY          TIME CMD
    1 ?        00:00:00 exe
    6 ?        00:00:00 bash
   11 ?        00:00:00 ps
```

本节完整代码如下所示

main.go - version 3

```go
package main

import (
	"fmt"
	"os"
	"os/exec"
	"syscall"
)

// go run main.go run <cmd> <args>
func main() {
	switch os.Args[1] {
	case "run":
		parent()
	case "child":
		child()
	default:
		panic("wat should I do")
	}
}

func parent() {
	fmt.Printf("Running %v \n", os.Args[2:])

	cmd := exec.Command("/proc/self/exe", append([]string{"child"}, os.Args[2:]...)...)
	cmd.SysProcAttr = &syscall.SysProcAttr{
		Cloneflags: syscall.CLONE_NEWUTS | syscall.CLONE_NEWPID | syscall.CLONE_NEWNS,
	}
	cmd.Stdin = os.Stdin
	cmd.Stdout = os.Stdout
	cmd.Stderr = os.Stderr

	must(cmd.Run())
}

func child() {

	cmd := exec.Command(os.Args[2], os.Args[3:]...)
	cmd.Stdin = os.Stdin
	cmd.Stdout = os.Stdout
	cmd.Stderr = os.Stderr

	must(syscall.Chroot("rootfs"))
	must(os.Chdir("/"))
	must(syscall.Mount("proc", "proc", "proc", 0, ""))

	must(cmd.Run())

	must(syscall.Mount("proc", "proc", "proc", 0, ""))
}

func must(err error) {
	if err != nil {
		panic(err)
	}
}
```

## 第五步：构建容器 - 添加 cgroup

目前为止，你已经有一个具有根文件系统，并且运行在一组隔离的命名空间中的进程。我们跳过了设置 cgroup，也跳过了对于根文件系统的管理，它可以让你有效地下载和缓存我们根文件系统映像。

我们也跳过了容器安装。你现在拥有的是一个在孤立的命名空间中的新容器。我们通过设置 rootfs 来定义 mount 命名空间，其他命名空间则有其默认的内容。在真正的容器中，我们需要在运行用户进程之前为容器配置 `world`。 比如，我们设置网络，在运行进程之前切换到正确的 uid，设置我们想要的任何其他限制（例如丢弃功能和设置rlimits）等等。 这可能会让我们超过100行。

下面指给出一个 cgroup 的示例代码。

```
func cg() {
	cgroups := "/sys/fs/cgroup/"
	pids := filepath.Join(cgroups, "pids")
	os.Mkdir(filepath.Join(pids, "liz"), 0755)
	must(ioutil.WriteFile(filepath.Join(pids, "liz/pids.max"), []byte("20"), 0700))
	// Removes the new cgroup in place after the container exits
	must(ioutil.WriteFile(filepath.Join(pids, "liz/notify_on_release"), []byte("1"), 0700))
	must(ioutil.WriteFile(filepath.Join(pids, "liz/cgroup.procs"), []byte(strconv.Itoa(os.Getpid())), 0700))
}
```

## 第六步：构建容器 - DONE!

到这一步，我们已经完成了一个超级超级简单的容器，代码还不到100行。 显然这是故意为止，你应该不想在生产系统中使用它。但是我觉得这个简单的程序已经可以为你勾勒出一幅有用的蓝图了。下面是最终代码。

main.go

```go
package main

import (
	"fmt"
	"os"
	"os/exec"
	"syscall"
)

// go run main.go run <cmd> <args>
func main() {
	switch os.Args[1] {
	case "run":
		parent()
	case "child":
		child()
	default:
		panic("wat should I do")
	}
}

func parent() {
	fmt.Printf("Running %v \n", os.Args[2:])

	cmd := exec.Command("/proc/self/exe", append([]string{"child"}, os.Args[2:]...)...)
	cmd.SysProcAttr = &syscall.SysProcAttr{
		Cloneflags: syscall.CLONE_NEWUTS | syscall.CLONE_NEWPID | syscall.CLONE_NEWNS,
	}
	cmd.Stdin = os.Stdin
	cmd.Stdout = os.Stdout
	cmd.Stderr = os.Stderr

	must(cmd.Run())
}

func child() {

	cmd := exec.Command(os.Args[2], os.Args[3:]...)
	cmd.Stdin = os.Stdin
	cmd.Stdout = os.Stdout
	cmd.Stderr = os.Stderr

	must(syscall.Chroot("rootfs"))
	must(os.Chdir("/"))
	must(syscall.Mount("proc", "proc", "proc", 0, ""))

	must(cmd.Run())

	must(syscall.Mount("proc", "proc", "proc", 0, ""))
}

func must(err error) {
	if err != nil {
		panic(err)
	}
}
```

## 附录：拓展阅读

[GOTO 2018 • Containers From Scratch • Liz Rice](https://www.youtube.com/watch?v=8fi7uSYlOdc)

[Build Your Own Container Using Less than 100 Lines of Go](https://www.infoq.com/articles/build-a-container-golang)

[DOCKER基础技术：LINUX NAMESPACE](https://coolshell.cn/articles/17010.html)

[mocker - A crappy imitation of Docker, written in 100% Python ](https://github.com/tonybaloney/mocker)


