# Simple Entity Component System

<hr>

Secs is a small, header-only ECS library meant to be as simple as possible.

<hr>

### ECS Architecture

Secs is a Sparse Set ECS, which lets you add and remove components from entities blazingly fast.

Entities are represented with an integer ID, and a generation number.
If you try to access an entity that has been killed, the generation
number will not match the internal generation component.

<hr>

### Usage

There are two primary ways to initialize the ECS Registry. Both are compile-time.

```cpp
EcsRegistry<Player, Enemy, Transform, Material, Renderable> registry;
```

As you can see, this can get out of hand quickly. The other way to initialize a registry is:

```cpp
EcsRegistry registry = EcsRegistry()
    .addComponent<Player>()
    .addComponent<Enemy>()
    .addComponent<Transform>()
    .addComponent<Material>()
    .addComponent<Renderable>();
```

This will instantiate a sparse set for each type, which you can access in the following ways:

```cpp
Component<Transform>& transforms = registry.getComponent<Transform>();

// Looping over dense data
for (int i = 0; i < transforms.size(); i++) {
    Transform t = transforms.data()[i];
}

// Accessing via sparse index
Transform playerTransform = transforms[playerEntity];
transforms[playerEntity] = Transform(0);
```

Entities are a simple struct with a sparse index ID, and a generation counter to safeguard against dead references.

```cpp
Entity enemy = registry.spawnEntity();
registry.addComponentToEntity<Enemy>(enemy, {.health = 100});
registry.getComponent<Enemy>()[enemy].health = 75;
registry.killEntity(enemy);
// Invalid, safeguarded, considering using assert() to automatically exit:
registry.getComponent<Enemy>()[enemy].health = 50;

// Potential future API (Entity stores a reference to ECS Singleton):
Entity enemy = registry.spawnEntity();
// Or
Entity enemy; // Default constructor calls registry.spawnEntity() under the hood.
enemy.addComponent<Enemy>(100);
enemy.getComponent<Enemy>().health = 75;
enemy.kill;
// Invalid.
enemy.getComponenet<Enemy>().health = 50;
```

```cpp
// Adding a component to an existing entity
registry.addComponentToEntity<Transform>(entity, transform);
```

<hr>

### Sparse Sets

A Sparse Set is a simple data structure that maps densely packed
`std::vector` data to sparse, stable indices.

Sparse Sets support O(1) insertion, deletion, access, and clearing.
Iteration is identical to that of an `std::vector` through the use of
`sset.data()`, and `sset.size()`. Iterators are currently in development.

<hr>

### Currently Supported Features:

- Entity generation validation.
- Entity reuse pooling.
- Compile-time, arbitrary custom Components.
- Archetype sorting, which aligns components in memory to optimize cache access.

<hr>

### Components

Components are the central piece on which this ECS is built.

The `EcsRegistry` only allows you to register one component per each compile-time type.
If you need two components with identical inner data, wrap them in separately named structs.
As an advantage of this approach, naming your components well is encouraged.

In this ECS, a Component is just a type alias on the SparseSet class. You must also 

<hr>

### Tags

Tags are simple, just make an empty struct component:

```cpp
struct DamagedThisFrame {};

EcsRegistry<Player, Enemy, DamagedThisFrame> registry;
```

<hr>

### Future Development

Star this library to support development!