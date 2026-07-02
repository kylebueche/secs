// Copyright (c) 2026 Kyle Bueche
// SPDX-License-Identifier: MIT
// Author: Kyle Bueche

#ifndef SECS_ECS_H_
#define SECS_ECS_H_

#include <cassert>
#include <cstdint>
#include <vector>
#include <type_traits>
#include <memory>

using EntityID = std::size_t;

struct Entity {
    EntityID id;
    uint64_t generation;
};

// A simple Sparse Set.
template <typename T>
struct SparseSet
{
    using DenseID = std::size_t;
    using SparseID = std::size_t;

    std::vector<DenseID> sparse;
    std::vector<SparseID> dense;
    std::vector<T> values;
    std::size_t numValues;

    SparseSet() = delete;

    explicit SparseSet(std::size_t capacity) :
        sparse(std::vector<DenseID>(capacity)),
        dense(std::vector<SparseID>(capacity)),
        values(std::vector<T>()),
        numValues(0) {
        // Initialize bijective mapping
        for (int i = 0; i < capacity; i++) {
            sparse[i] = i;
            dense[i] = i;
        }
        values.reserve(capacity);
    }

    std::size_t sparseSize() const {
        return sparse.size();
    }
    std::size_t sparseCapacity() const {
        return sparse.capacity();
    }

    // Vector-like functions
    void clear() {
        values.clear();
    }
    T& operator[](std::size_t index) {
        return values[sparse[index]];
    }
    const T& operator[](std::size_t index) const {
        return values[sparse[index]];
    }
    std::size_t size() const {
        return values.size();
    }
    std::size_t capacity() const {
        return values.capacity();
    }
    T* data() {
        return values.data();
    }
    void reserve(std::size_t requiredSize) {
        if (requiredSize <= capacity()) return;
        std::size_t oldSparseSize = sparseSize();
        sparse.resize(requiredSize);
        dense.resize(requiredSize);
        values.reserve(requiredSize);
        // Maintain bijective mapping
        for (std::size_t i = oldSparseSize; i < sparseSize(); i++) {
            sparse[i] = i;
            dense[i] = i;
        }
    }
    void resize(std::size_t newCapacity) {
        reserve(newCapacity);
        values.resize(newCapacity);
    }

    [[nodiscard]] bool isValidSparseID(SparseID sparseID) const {
        return sparseID < sparseSize() && sparse[sparseID] < size();
    }

    // Main tooling
    SparseID push_back(T value) {
        reserve(size() + 1);
        SparseID sparseID = dense[size()];
        values.push_back(value);
        return sparseID;
    }

    // TODO: Fix functionality.
    // Current issue:
    // swapDense() only works if both DenseIDs are less than size()
    // Inserting at an unused sparse index is guaranteed to yield
    // a dense index greater than or equal to size.
    // Desired behavior:
    // values.push_back(),
    // followed by making the specific sparse ID point to size() - 1
    // This can be achieved by locating the Dense ID at the tail,
    // and taking the desired Sparse ID, finding their counterparts,
    // and swapping the Sparse & Dense IDs respectively.
    // this can be a new function called swapSparse(i, j).
    // This is only valid when SparseIDs i and j have no value data.
    // If allocating an unused sparse ID, both it and the tail at size()
    // are guaranteed to have no value data
    // Note: Large sparseIDs can cause a huge resize
    bool insert(SparseID sparseID, T value) {
        if (isValidSparseID(sparseID)) return false;
        reserve(sparseID + 1);
        push_back(value);

        swapSparse(dense[size()], sparseID);
        return true;
    }


    void remove(SparseID sparseID) {
        if (!isValidSparseID(sparseID)) return;

        DenseID denseID = sparse[sparseID];
        DenseID tailDenseID = size() - 1;

        swapDense(denseID, tailDenseID); // Swap...
        values.pop_back(); // & pop!
    }

    T pop_back() {
        assert(size() > 0);
        return values.pop_back();
    }

private:
    void swapDense(DenseID i, DenseID j) {
        std::swap(sparse[dense[i]], sparse[dense[j]]);
        std::swap(dense[i], dense[j]);
        std::swap(values[i], values[j]);
    }
    void swapSparse(SparseID i, SparseID j) {
        std::swap(dense[sparse[i]], dense[sparse[j]]);
        std::swap(sparse[i], sparse[j]);
    }
};

template <typename T>
using Component = SparseSet<T>;

template <class... ComponentTypes>
class EcsRegistry
{
public:
    struct ECS_GENERATION
    {
        uint64_t generation;
    };

    std::tuple<Component<ECS_GENERATION>, Component<ComponentTypes>...> componentList;
    std::vector<EntityID> reusePool;

    template <typename T>
    auto addComponent() {
        static_assert(!(std::is_same_v<ComponentTypes, T> || ...));
        return EcsRegistry<ComponentTypes..., std::decay_t<T>>(
            std::move(std::tuple_cat(std::move(componentList), std::tuple<Component<T>>())));
    }

    template <typename T>
    Component<T>& getComponent() {
        return std::get<T>(componentList);
    }

    bool isValidEntity(Entity entity) {
        return entity.generation == getComponent<ECS_GENERATION>()[entity.id].generation;
    }

    Entity spawnEntity() {
        Component<ECS_GENERATION>& generations = getComponent<ECS_GENERATION>();
        if (reusePool.empty()) {
            EntityID entityID = reusePool.back();
            reusePool.pop_back();
            return Entity(entityID, generations[entityID].generation);
        }
        EntityID entityID = generations.push_back(ECS_GENERATION{.generation=0});
        return Entity(entityID, 0);
    }

    bool killEntity(Entity entity) {
        if (!isValidEntity(entity)) return false;
        getComponent<ECS_GENERATION>()[entity.id].generation += 1;
        reusePool.push_back(entity.id);
        (getComponent<ComponentTypes>().remove(entity), ...);
        return true;
    }

    template <typename ComponentType>
    void addComponentToEntity(Entity entity, ComponentType value) {
        getComponent<ComponentType>().insert(entity.id, value);
    }

    // Aligns an archetype of components in memory so that
    // the dense data is contiguous in an SoA format.
    // Returns the size of the archetype.
    template <typename... Components>
    std::size_t alignArchetype() {
        std::size_t minSize = min(getComponent<Components>().size() ...);
        std::size_t archetypeSize = 0;

        auto iterateSmallestList = [&](auto& component) {
            if (component.size() != minSize)
                return false;

            for (std::size_t i = 0; i < minSize; i++) {
                EntityID entityID = component.dense[i];
                if ((getComponent<Components>().isRegistered(entityID) && ...)) {
                    (getComponent<Components>().swapDense(i, archetypeSize), ...);
                    archetypeSize++;
                }
            }
            return true;
        };

        (iterateSmallestList(getComponent<Components>()) || ...);
        return archetypeSize;
    }


};

// Experimental
template <typename... types>
struct VectorSoA
{
    std::tuple<std::vector<types>...> vectors;
    std::size_t size() const { return std::get<0>(vectors).size(); }
    std::size_t capacity() const { return std::get<0>(vectors).capacity(); }
    auto data(std::size_t index) { return std::get<index>(vectors); }
    void push_back(types... vals) {
        auto tup = std::forward_as_tuple(vals...);
        for (int i = 0; i < size(); i++) {
            std::get<i>(vectors).push_back(std::get<i>(tup));
        }
    }
    void pop_back() {
        for (int i = 0; i < size(); i++) {
            std::get<i>(vectors).pop_back(std::get<i>(vectors));
        }
    }
};

#endif //SECS_ECS_H_