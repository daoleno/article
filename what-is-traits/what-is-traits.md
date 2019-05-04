# 当我们在说 Traits 的时候，我们到底在指什么？

很多语言都有 `Traits` 这个关键字，看起来这是一个很重要的特性。但是这个特性究竟在各个语言中意味着什么呢？

## C++ Traits

> 下面的代码实例来自 《C++ Templates》

### 一个实例：累加一个序列

首先我们假设用来累加的值都是存在一个数组里的，并且我们具有一个指向数组首元素的指针，和一个指向数组最后一个元素的后一位的指针。这两个指针之间的元素就是我们要求和的元素。下面是一个累加的实例，我们希望能够对多种类型的数据进行求和，所以使用了模板。

```cpp
// traits/accum1.hpp
#ifndef ACCUM_HPP
#define ACCUM_HPP
template <typename T>
inline T accum(T const* beg, T const* end) {
  T total = T();  // assume T() actually creates a zero value
  while (beg != end) {
    total += *beg;
    ++beg;
  }
  return total;
}
#endif  // ACCUM_HPP
```

在上面的示例中，一个稍微复杂的点就是如何为正确类型创建一个 0 值，以便开始我们的求和运算。我们在这里使用了表达式 `T()` ，对于内部数值类型 `int` 和 `float` 应该是可以正常工作的。

下面来看看怎么使用这个模板：

```cpp
// traits/accum1.cpp
#include "accum1.hpp"
#include <iostream>
int main() {
  // create array of 5 integer values
  int num[] = {1, 2, 3, 4, 5};
  // print average value
  std::cout << "the average value of the integer values is "
            << accum(&num[0], &num[5]) / 5 << '\n';
  // create array of character values
  char name[] = "templates";
  int length = sizeof(name) - 1;
  // (try to) print average character value
  std::cout << "the average value of the characters in \"" << name << "\" is "
            << accum(&name[0], &name[length]) / length << '\n';
}
```

在程序的上半部分，我们使用 `accum()` ，并除以元素数量获得了一个整型数组元素的平均值。

接下来，我们企图对存储着 `templates` 的字符数组做同样的事情，我们期望的结果是 `a` 到 `z` 之间的一个字母，在现今的大多数平台上，这些字符是由 ASCII 码来表示的(`a` 为 91, `z` 为 122)，所以我们的理想结果是得到一个位于 91 到 122 之间的数。然而，在我的平台上，输出如下：

```zsh
the average value of the integer values is 3
the average value of the characters in "templates" is -5
```
这里的问题在于我们的模板是基于 `char` 类型进行实例化的，而 char 的范围是很小的（-127 to 127 or 0 to 255），即使是相对较小的数值进行求和，也会发生越界的情况。

显然，我们可以引入一个额外的模板参数 `AccT` 来描述变量 `total` 的类型（同时也是返回类型）。但这也会加重用户的负担：每次调用模板时都得指定额外的类型。比如说像下面这样：

```cpp
accum<int>(&name[0], &name[length])
```

虽然说这个约束不是很麻烦，但是我们还是可以避免的。

一个可选的方案就是为每个可能被 `accum()` 调用的类型 `T` 与其所对应的用来存储累加值的类型创建一个关联。这种关联被称作类型 `T` 的特性，因此我们也把这个存储累加值的类型叫做 `T` 的 `trait`。于是，我们可以用模板特化来写出这些关联代码：

```cpp
// traits/accumtraits2.hpp
template <typename T>
class AccumulationTraits;
template <>
class AccumulationTraits<char> {
 public:
  typedef int AccT;
};
template <>
class AccumulationTraits<short> {
 public:
  typedef int AccT;
};
template <>
class AccumulationTraits<int> {
 public:
  typedef long AccT;
};
template <>
class AccumulationTraits<unsigned int> {
 public:
  typedef unsigned long AccT;
};
template <>
class AccumulationTraits<float> {
 public:
  typedef double AccT;
};
```

模板 `AccumulationTraits` 被称作 `traits` 模板，因为它含有它的参数类型的 `trait`。我们这里不提供一个泛型定义是因为，在我们不知道参数类型是什么的前提下，不好确认累加的类型。然而，我们可以利用 `T` 这个实参类型。

有了这个概念后，我们可以像下面这样重写 `accum()` 模板：

```cpp
// traits/accum2.hpp
#ifndef ACCUM_HPP
#define ACCUM_HPP
#include "accumtraits2.hpp"
template <typename T>
inline typename AccumulationTraits<T>::AccT accum(T const* beg, T const* end) {
  // return type is traits of the element type
  typedef typename AccumulationTraits<T>::AccT AccT;
  AccT total = AccT();  // assume T() actually creates a zero value
  while (beg != end) {
    total += *beg;
    ++beg;
  }
  return total;
}
#endif  // ACCUM_HPP
```

现在程序的输入将如下所示：

```
the average value of the integer values is 3
the average value of the characters in "templates" is 108
```

可以看到，输出符合我们一开始的预期。

我们其实已经添加了一套相当有用的机制来自定义我们的算法。更进一步讲，如果有新的类型需要使用 `accum()`，只需要声明 `AccumulationTraits` 模板的一个新的显示特化来关联这个类型就好了。

### Traits 机制

上面只是给出了 Traits 作为关联类型所起的作用。实际上，traits 就是一种机制，可以提供 `accum()` 所需要的，关于元素类型的所有必要信息；这个元素类型就是调用 accum() 的类型，即模板参数的类型。

**关键概念：**

`Traits` 提供了一种配置具体元素（通常是类型）的途径，该途径主要用于泛型计算。

### type_traits

C++11 标准库中引入了 <type_traits>, 方便我们在元编程时获取一些类型的特征信息，并根据这些信息选择应有的操作。

比如我们可以使用 Type Traits 来判断类型的特性：

```cpp
#include <type_traits>
template<typename T>
constexpr bool is_pod(T) {
    return std::is_pod<T>::value;
}
```

这里定义了一个 is_pod() 的函数模板，对 type_traits 中的 is_pod 进行了简单的封装，可以通过这个函数来判断数据是否为 POD 类型的：

```cpp
int main(){
    int a;
    std::cout << is_pod(a) << std::endl;
}
```

这里的要点在于 type_traits 是用于元编程的，我们的函数又用了 constexpr 来修饰，所以该函数在编译器就可以获得 is_pod() 的返回值。

更多 type_traits 提供的特性可以在[标准库](https://en.cppreference.com/w/cpp/header/type_traits)里找到

## Rust Traits

Rust 中 Traits 是其抽象行为的精华所在，类似于某些编程语言中的接口。

`trait` 是对未知类型定义的方法集：`Self`。它们可以访问同一个 trait 中定义的方法。

> 下面的代码实例来源于 《rust by example》

### 一个实例：Animal Traits

Traits 可以被实现为任何数据类型。下面给出一个 traits 基本用法的实例。

```rust
struct Sheep { naked: bool, name: &'static str }

trait Animal {
    // Static method signature; `Self` refers to the implementor type.
    fn new(name: &'static str) -> Self;

    // Instance method signatures; these will return a string.
    fn name(&self) -> &'static str;
    fn noise(&self) -> &'static str;

    // Traits can provide default method definitions.
    fn talk(&self) {
        println!("{} says {}", self.name(), self.noise());
    }
}

impl Sheep {
    fn is_naked(&self) -> bool {
        self.naked
    }

    fn shear(&mut self) {
        if self.is_naked() {
            // Implementor methods can use the implementor's trait methods.
            println!("{} is already naked...", self.name());
        } else {
            println!("{} gets a haircut!", self.name);

            self.naked = true;
        }
    }
}

// Implement the `Animal` trait for `Sheep`.
impl Animal for Sheep {
    // `Self` is the implementor type: `Sheep`.
    fn new(name: &'static str) -> Sheep {
        Sheep { name: name, naked: false }
    }

    fn name(&self) -> &'static str {
        self.name
    }

    fn noise(&self) -> &'static str {
        if self.is_naked() {
            "baaaaah?"
        } else {
            "baaaaah!"
        }
    }
    
    // Default trait methods can be overridden.
    fn talk(&self) {
        // For example, we can add some quiet contemplation.
        println!("{} pauses briefly... {}", self.name, self.noise());
    }
}

fn main() {
    // Type annotation is necessary in this case.
    let mut dolly: Sheep = Animal::new("Dolly");
    // TODO ^ Try removing the type annotations.

    dolly.talk();
    dolly.shear();
    dolly.talk();
}
```

上面的代码中，我们通过 `trait Animal` 声明了一个 trait，表明 Animal 包含一组实现某些目的所必须的行为的集合，这就是 trait 所谓共享行为的概念。

接着我们通过 `impl Animal for Sheep` 在 Sheep 类型上实现 trait。在 impl 块中，使用 trait Animal 定义中的方法签名，不过不再后跟分号，而是需要在大括号中编写函数体来为特定类型(Sheep)实现 trait 方法所拥有的行为。

一旦实现了 trait，我们就可以在 main() 函数中对 Sheep 的实例调用共享方法来实现对应的行为。

可以看到，traits 为 rust 提供了灵活的泛型特性。**Rust 抽象的基础是 traits:**

* Traits 是 Rust 唯一的接口抽象方式。
* Traits 可以被静态分配。
    > 类似 C++ 模板，你可以让编译器为不同种类型的同一抽象静态生成不同的代码。
* Traits 可以被动态分配。
    > 有时你确实需要在运行时调用某种间接抽象，trait 也提供了动态分配（Dynamic Dispatch）的机制。
* Traits 除了简单的抽象还能解决大量额外的问题。
    > 可以被用做类型标记(`markers`)。也可以用来定义拓展方法(`extension methods`)--也就是对已有类型添加拓展方法。

## 总结

总的来看，C++ traits 和 Rust traits 是两个完全不同的概念。C++ traits 是元编程中类型的特性，Rust traits 是 Rust 提供的一种抽象机制。不过，[C++ concepts](https://en.cppreference.com/w/cpp/language/constraints) 倒是和 Rust traits 许多有相像之处，有兴趣可以进一步了解一下。