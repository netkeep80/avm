<p align="center"><img src="EOSR.jpg"></p>

# avm

```
      Associative Virtual Machine

                  E
                  ^
                  |
                  E
                  |
                  +
           E+-----R----->E
           
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

avm uses the following third party tools. Thanks a lot!

- [**JSON for Modern C++**](https://github.com/nlohmann/json) for load/unload json view of Relations Model
- [**LinksPlatform**](https://github.com/linksplatform) for store associative model
- [**ChatGPT 4.0**](https://chat.openai.com/chat) for documentation generation

Associative Virtual Machine
Associative Virtual Machine (AVM) является проектом, целью которого является реализация виртуальной машины для исполнения кода, основанного на модели ассоциативных отношений (МАО). Проект предоставляет инструменты для работы с МАО, включая представление данных, функций и логических операций.

Проект написан на языке C++ и использует систему сборки CMake.

Особенности
Реализация виртуальной машины для исполнения кода, основанного на МАО.
Поддержка структурированных данных, таких как массивы и ассоциативные массивы.
Представление логических операций и функций с аргументами.
Использование многомерной относительной адресации для передачи аргументов и получения результатов функций.
Установка
Клонируйте репозиторий:
bash
Copy code
git clone https://github.com/netkeep80/avm.git
Перейдите в директорию проекта:
bash
Copy code
cd avm
Создайте директорию сборки:
bash
Copy code
mkdir build && cd build
Запустите CMake для генерации Makefile:
Copy code
cmake ..
Скомпилируйте проект:
go
Copy code
make
Использование
Запуск виртуальной машины
bash
Copy code
./avm
Работа с данными и функциями
Примеры работы с данными и функциями с использованием AVM можно найти в документации и в файлах примеров проекта.

Документация
Дополнительную информацию о модели ассоциативных отношений (МАО) и AVM можно найти в документации проекта:

docs/mao_theory.md: Теория модели ассоциативных отношений (МАО).
docs/avm_guide.md: Руководство по использованию Associative Virtual Machine.
Лицензия
Associative Virtual Machine распространяется под лицензией MIT. Подробности см. в файле LICENSE.

Контакты
Если у вас возникли вопросы или предложения, не стесняйтесь обращаться по адресу: netkeep80@gmail.com