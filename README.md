<p align="center"><img src="EOSR.jpg"></p>

# avm

```
      Associative Virtual Machine
             _____________
            /             \
           /               \
          /                 +
     ,--> E +---------------O----.
    /     |                 A     \
   /      |  E = O x S x R  |      \
   |      |  O = E x R x S  |      |
   |      |  S = R x E x O  |      |
   \      |  R = S x O x E  |      /
    \     V                 |     /
     `----S---------------+ R <--`
          +                 |
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

# Ассоциативная модель отношений (АМО)

Ассоциативная модель отношений (АМО) представляет собой математическую модель для хранения описания соответствия между множествами и сопоставления элементов множеств.

## Определение

АМО определяется как упорядоченное множество ассоциаций A. Каждая ассоциация есть кортеж длины 2 из элементов множества L:
```
L = {l1, l2, l3, ...}
A = {(a1, a2) | a1, a2 ∈ L}
```

Множество L содержит уникальные идентификаторы элементов множества A. Отображение L в A инъективно.

Ассоциации интерпретируются как сущности (E) или как отношения (R) в зависимости от контекста их использования:

- Интерпретация ассоциации как сущность (E) интерпретирует кортеж как соответствие между отношением и сущностью, т.е. (R, E).
- Интерпретация ассоциации как отношение (R) интерпретирует кортеж как отображение между двумя сущностями, т.е. (R, R).

## Начальные ассоциации и множества

Для начального создания ассоциаций и их интерпретации необходимо определить две ассоциации: Ent и Rel. Эти ассоциации определяют 2 главных множества внутри АМО:
```
Rel = (Ent, Ent) — ассоциация описывающая множество отношений внутри АМО;
Ent = (Rel, Ent) — ассоциация описывающая множество сущностей внутри АМО.
```

Все кортежи, второй компонент которых равен Ent или Rel, интерпретируются как сущности.

## Примеры использования АМО

АМО может быть использована для представления и хранения различных видов информации, таких как графы знаний, базы данных, семантические сети и других структур данных. Возможные применения АМО включают:

1. **Графы знаний**: АМО может быть использована для представления графов знаний, где сущности и отношения представлены в виде ассоциаций, а функция интерпретации определяет их семантическое значение.

2. **Базы данных**: В контексте баз данных, АМО может использоваться для представления таблиц и отношений между ними. Сущности и отношения могут быть представлены с помощью ассоциаций, а запросы к базе данных могут быть выполнены путем обработки и сопоставления кортежей.

3. **Семантические сети**: АМО может быть применена для представления семантических сетей, в которых концепты и связи между ними представлены с помощью ассоциаций. АМО позволяет легко определить структуру сети и осуществлять поиск и анализ информации.

4. **Формализация знаний**: С помощью АМО можно формализовать знания в виде структурированных данных, что облегчает их обработку, хранение и обмен. АМО предоставляет унифицированный формат представления знаний, что упрощает их интеграцию в различные приложения.

5. **Искусственный интеллект и машинное обучение**: АМО может быть использована в области искусственного интеллекта и машинного обучения для представления и обработки знаний. АМО позволяет эффективно представлять и хранить знания, что может быть полезно для обучения и решения задач в области ИИ.

## Заключение

Ассоциативная модель отношений (АМО) является мощным и гибким инструментом для представления и хранения информации. Она обеспечивает унифицированный подход к представлению сущностей и отношений между ними, что может быть полезно во многих областях, таких как графы знаний, базы данных, семантические сети, формализация знаний и искусственный интеллект. АМО предлагает компактное и эффективное представление данных, что облегчает их хранение, обработку и обмен между различными системами и приложениями.