# avm

```
      Associative Virtual Machine
             _____________
            /             \
           /               \
          /                 V
     ,--> E +---------------O----.
    /     |                 +     \
   /      |  E = S x O x R  |      \
   |      |  R = O x S x E  |      |
   |      |  S = R x E x O  |      |
   \      |  O = E x R x S  |      /
    \     +                 |     /
     `----S---------------+ R <--`
          A                 |
           \               /
            \_____________/

```

## License

<img align="right" src="https://opensource.org/trademarks/opensource/OSI-Approved-License-100x137.png">

[MIT License](https://opensource.org/licenses/MIT)

Copyright &copy; 2022 [Vertushkin Roman Pavlovich](https://vk.com/earthbirthbook)

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the “Software”), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

## Thanks

I deeply appreciate the help of the following people.

- [Vladimir Muravyev](https://github.com/vmuravyev) helped to develop the terminological apparatus of the Relations Model.

## Used third-party tools

avm using the following third-party tools. Thanks a lot!

- [**JSON for Modern C++**](https://github.com/nlohmann/json) for load/unload json view of Relations Model
- [**LinksPlatform**](https://github.com/linksplatform) for store associative model
