# ProjectionDescription: независимая от parser граница `find` / `realize`

`ProjectionDescription` — нейтральная граница между внешним протоколом или проекцией модели и хранилищем AVM. Она намеренно не содержит JSON syntax, Anum abits, состояние parser, правила quotation или execution semantics.

## Почему projection points не создаются неявно

`InMemoryLinkStore::create_point()` создаёт уникальную identity, физически представленную self-link `{id,id}`. Две точки различны, хотя обе имеют одинаковую структурную форму self-link.

Read-only query не может корректно «найти анонимную точку» только по self-link: таких связей может быть много, и ни одна структурно не предпочтительнее другой. Скрытый external-name→point map внутри `ProjectionDescription` превратил бы memory layer в resolver identity.

Поэтому используются два явных вида references:

```text
Anchor(LinkId)    identity уже разрешена внешним adapter/context
Node(NodeId)      результат более раннего дуплета в той же проекции
```

Сама projection boundary никогда не вызывает `create_point()`.

## Граф описания

Проекция — топологически упорядоченный список дуплетов и root reference:

```text
node[0] = Link(Anchor(a), Anchor(b))
node[1] = Link(Node(0),   Anchor(c))
root    = Node(1)
```

Node может ссылаться только на более ранний node. Это соответствует immutable construction model `LinkStore` и превращает cycles/forward references в явные ошибки валидации, а не в частично материализованное состояние.

Root может быть и `Anchor`, в том числе при пустом списке nodes.

## `find_projection`

```text
find_projection(const LinkStore&, description)
    -> optional<ProjectionResult>
```

Операция является query-only и по типу, и по поведению:

1. валидирует структуру description;
2. разрешает anchors через `contains`;
3. разрешает каждый дуплет только через `find(begin,end)`;
4. немедленно возвращает отсутствие, если необходимого anchor или дуплета нет;
5. возвращает существующие canonical `LinkId`, если вся проекция уже существует.

Она никогда не вызывает `create_point` или `intern`. Тесты сравнивают размер store до и после успешных и неуспешных запросов.

## `realize_projection`

```text
realize_projection(LinkStore&, description)
    -> ProjectionResult
```

Это явная изменяющая операция:

1. валидирует всю проекцию до записи;
2. проверяет **все anchors** до записи;
3. обрабатывает nodes в топологическом порядке через `intern(begin,end)`;
4. переиспользует существующие canonical links;
5. возвращает root и `LinkId` каждого node.

Предварительная проверка anchors не позволяет обычной ошибке missing-anchor материализовать корректный prefix и затем упасть посередине description.

Повторный `realize_projection` над тем же description и теми же anchors идемпотентен благодаря canonical `intern`.

## Владение идентичностью

Внешний adapter/context отвечает на вопрос: «какая существующая AVM identity соответствует этой protocol-level сущности?» — и после разрешения передаёт `Anchor(LinkId)`.

Если будущему протоколу потребуется persistent external-symbol→point creation, для этого нужен отдельный явный resolver contract. Его нельзя прятать в query semantics, иначе нарушится инвариант `find` не создаёт.

## Связь с Anum/МТС

Этот слой соответствует границе L3/L4, а не syntax Anum:

```text
сырой внешний источник
  -> внешний parse / validate / quote / project(context)
  -> ProjectionDescription
  -> AVM find_projection | realize_projection
  -> каноническая денотация в LinkStore
```

`anum_docs` предоставляет каноническую L3 semantics. AVM знает только завершённое структурное description и уже разрешённые anchors.
