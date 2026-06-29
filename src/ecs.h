// Copyright (c) 2026 Kyle Bueche.
// Author: Kyle Bueche.
// This project is licensed under the MIT Licence - see LICENSE.txt.
// No warranty implied.

#ifndef SECS_ECS_H_
#define SECS_ECS_H_

#include <stdint.h>
#include <vector>
#include <type_traits>
#include <memory>

using EntityID = uint32_t;

// A simple Sparse Set.
// This structure does not resize on insert().
// Resizing must be done explicitely.
template <typename T>
struct SparseSet
{
    using DenseID = std::size_t;
    using SparseID = std::size_t;

    std::vector<DenseID> sparse;
    std::vector<SparseID> dense;
    std::vector<T> values;
    std::size_t numValues;
    std::size_t capacity;

    SparseSet() = delete;

    explicit SparseSet(std::size_t capacity) :
        capacity(capacity),
        sparse(std::vector<DenseID>(capacity)),
        dense(std::vector<SparseID>(capacity)),
        values(std::vector<T>(capacity)),
        numValues(0) {
        // Start mapping bijective
        for (int i = 0; i < capacity; i++) {
            sparse[i] = i;
            dense[i] = i;
        }
    }

    void assure_capacity(std::size_t requiredSize) {
        if (requiredSize <= capacity) return;
        // Grow exponentially
        std::size_t newCapacity = std::max(requiredSize, std::size_t(capacity * 1.5));
        sparse.resize(newCapacity);
        dense.resize(newCapacity);
        values.resize(newCapacity);
        // Maintain bijective mapping
        for (std::size_t i = capacity; i < newCapacity; i++) {
            sparse[i] = i;
            dense[i] = i;
        }
        capacity = newCapacity;
    }

    // Vector-like functions
    void clear() {
        numValues = 0;
    }
    std::size_t size() {
        return numValues;
    }
    T& operator[](std::size_t index) {
        return values[sparse[index]];
    }
    const T& operator[](std::size_t index) const {
        return values[sparse[index]];
    }
    T* data() {
        return values.data();
    }
    void resize(std::size_t newCapacity) {
        sparse.resize(newCapacity);
        dense.resize(newCapacity);
        values.resize(newCapacity);
        capacity = newCapacity;
        if (numValues > capacity) {
            numValues = newCapacity;
        }
    }

    [[nodiscard]] bool isRegistered(SparseID sparseID) const {
        return sparseID < capacity && sparse[sparseID] < numValues;
    }
    [[nodiscard]] bool isFull() const {
        return numValues >= capacity;
    }

    // Main tooling
    SparseID insert(T value) {
        assure_capacity(numValues + 1);

        SparseID sparseID = dense[numValues];
        values[numValues] = std::move(value);
        numValues++;

        return sparseID;
    }

    bool insertAt(SparseID sparseID, T value) {
        if (isRegistered(sparseID)) return false;
        assure_capacity(sparseID + 1);

        DenseID currentDensePos = sparse[sparseID];
        std::swap(dense[numValues], dense[currentDensePos]);
        sparse[dense[currentDensePos]] = currentDensePos;
        sparse[dense[numValues]] = numValues;
        values[numValues] = std::move(value);
        numValues++;
        return true;
    }

    void remove(SparseID sparseID) {
        if (!isRegistered(sparseID)) return;

        DenseID denseID = sparse[sparseID]; // Store the dense index
        DenseID tailDenseID = numValues - 1; // Find the tail entry of the dense list.
        SparseID tailSparseID = dense[tailDenseID]; // Find its sparse ID

        std::swap(dense[denseID], dense[tailDenseID]); // Swap
        std::swap(values[denseID], values[tailDenseID]); // ..
        numValues--; // & pop!

        sparse[tailSparseID] = denseID; // Point the old tail's sparse ID to its new dense index
        sparse[sparseID] = tailDenseID;
    }
};

template <typename T>
using Component = SparseSet<T>;

template <class... ComponentTypes>
class EcsRegistry
{
public:
    std::tuple<Component<ComponentTypes>...> componentList;

    template <typename T>
    auto registerComponent() {
        return EcsRegistry<ComponentTypes..., std::decay_t<T>>(std::move(std::tuple_cat(std::move(componentList), std::tuple<Component<T>>())));
    }

    template <typename T>
    Component<T>& getComponent() {
        return std::get<T>(componentList);
    }
};

#endif //SECS_ECS_H_