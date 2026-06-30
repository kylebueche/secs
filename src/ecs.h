// Copyright (c) 2026 Kyle Bueche
// SPDX-License-Identifier: MIT
// Author: Kyle Bueche

#ifndef SECS_ECS_H_
#define SECS_ECS_H_

#include <cstdint>
#include <vector>
#include <type_traits>
#include <memory>

using EntityID = std::size_t;

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
    }

    bool insertAt(SparseID sparseID, T value) {
        if (isValidSparseID(sparseID)) return false;
        reserve(sparseID + 1);

        DenseID currentDensePos = sparse[sparseID];
        swapDense(numValues, currentDensePos);
        std::swap(dense[numValues], dense[currentDensePos]);
        sparse[dense[numValues]] = numValues;
        values[numValues] = std::move(value);
        numValues++;
        return true;
    }

    void remove(SparseID sparseID) {
        if (!isValidSparseID(sparseID)) return;

        DenseID denseID = sparse[sparseID];
        DenseID tailDenseID = numValues - 1;

        swapDense(denseID, tailDenseID); // Swap...
        values.pop_back(); // & pop!
    }

private:
    void swapDense(DenseID i, DenseID j) {
        std::swap(sparse[dense[i]], sparse[dense[j]]);
        std::swap(dense[i], dense[j]);
        std::swap(values[i], values[j]);
    }
};

template <typename T>
using Component = SparseSet<T>;

template <class... ComponentTypes>
class EcsRegistry
{
public:
    struct ECS_BUILTIN_EntityAlive {};

    std::tuple<Component<ECS_BUILTIN_EntityAlive>, Component<ComponentTypes>...> componentList;

    template <typename T>
    auto registerComponent() {
        static_assert(!(std::is_same_v<ComponentTypes, T> || ...));
        return EcsRegistry<ComponentTypes..., std::decay_t<T>>(
            std::move(std::tuple_cat(std::move(componentList), std::tuple<Component<T>>())));
    }

    template <typename T>
    Component<T>& getComponent() {
        return std::get<T>(componentList);
    }

    EntityID newEntity() {
        getComponent<ECS_BUILTIN_EntityAlive>().insert({});
    }

    void killEntity(EntityID entity) {
        getComponent<ECS_BUILTIN_EntityAlive>().remove(entity);
        (getComponent<ComponentTypes>().remove(entity), ...);
    }

    // Aligns an archetype of components in memory so that
    // the dense data is contiguous in an SoA format.
    // Returns the size of the archetype.
    template <typename... Components>
    std::size_t align() {
        std::size_t minSize = min(getComponent<Components>().size() ...);
        std::size_t archetypeSize = 0;

        auto iterateSmallestList = [&](auto& component) {
            if (component.size() == minSize) {
                for (std::size_t i = 0; i < minSize; i++) {
                    EntityID entityID = component.dense[i];
                    if ((getComponent<Components>().isRegistered(entityID) && ...)) {
                        (getComponent<Components>().swapDense(i, archetypeSize), ...);
                        archetypeSize++;
                    }
                }
                return true;
            }
            return false;
        };

        (iterateSmallestList(getComponent<Components>()) || ...);
        return archetypeSize;
    }


};

#endif //SECS_ECS_H_