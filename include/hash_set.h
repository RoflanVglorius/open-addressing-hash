#pragma once
#include "policy.h"

#include <memory>
#include <optional>
#include <stdexcept>
#include <vector>

template <
        class Key,
        class CollisionPolicy = LinearProbing,
        class Hash = std::hash<Key>,
        class Equal = std::equal_to<Key>>
class HashSet
{

public:
    // types

    class SetIterator;

    using key_type = Key;
    using value_type = Key;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;
    using hasher = Hash;
    using key_equal = Equal;
    using reference = value_type &;
    using const_reference = const value_type &;
    using pointer = value_type *;
    using const_pointer = const value_type *;

    using iterator = SetIterator;
    using const_iterator = SetIterator;

    explicit HashSet(size_type expected_max_size = 0,
                     const hasher & hash = hasher(),
                     const key_equal & equal = key_equal())
        : set_max_size(expected_max_size)
        , hash_set(expected_max_size * 2)
        , deleted(expected_max_size * 2, true)
        , set_hasher(hash)
        , set_equal(equal)
    {
    }

    template <class InputIt>
    HashSet(InputIt first, InputIt last, size_type expected_max_size = 0, const hasher & hash = hasher(), const key_equal & equal = key_equal())
        : HashSet(expected_max_size, hash, equal)
    {
        for (auto it = first; it != last; ++it) {
            insert(*it);
        }
    }

    HashSet(const HashSet & other) = default;

    HashSet(HashSet &&) = default;

    HashSet(std::initializer_list<value_type> init,
            size_type expected_max_size = 0,
            const hasher & hash = hasher(),
            const key_equal & equal = key_equal())
        : HashSet(init.begin(), init.end(), expected_max_size, hash, equal){};

    HashSet & operator=(const HashSet & other)
    {
        return *this = HashSet(other);
    }

    HashSet & operator=(HashSet && new_set) noexcept
    {
        swap(new_set);
        return *this;
    };

    HashSet & operator=(std::initializer_list<value_type> init)
    {
        return *this = HashSet(init, init.size());
    };

    ~HashSet() = default;

    iterator begin() noexcept
    {
        return iterator(&hash_set, &deleted, first_ind);
    }

    const_iterator begin() const noexcept
    {
        return cbegin();
    }

    const_iterator cbegin() const noexcept
    {
        return const_iterator(&hash_set, &deleted, first_ind);
    }

    iterator end() noexcept
    {
        return iterator(&hash_set, &deleted, set_max_size * 2);
    }

    const_iterator end() const noexcept
    {
        return cend();
    }

    const_iterator cend() const noexcept
    {
        return const_iterator(&hash_set, &deleted, set_max_size * 2);
    }

    bool empty() const
    {
        return set_size == 0;
    }

    size_type size() const
    {
        return set_size;
    }

    size_type max_size() const
    {
        return set_max_size;
    }

    void clear()
    {
        hash_set.clear();
        deleted.clear();
        std::vector<std::optional<value_type>> temp_set(set_max_size * 2);
        std::vector<bool> temp_deleted(set_max_size * 2, true);
        std::swap(hash_set, temp_set);
        std::swap(deleted, temp_deleted);
        set_size = 0;
        first_ind = 0;
    }

    std::pair<iterator, bool> insert(const value_type & key)
    {
        check_fullness();
        size_type insert_ind = find_index(key, true);
        bool inserted = false;
        if (!hash_set[insert_ind] || deleted[insert_ind]) {
            inserted = true;
            hash_set[insert_ind].template emplace(key);
            update_after_insert(insert_ind);
        }
        return std::make_pair(iterator(&hash_set, &deleted, insert_ind), inserted);
    }

    std::pair<iterator, bool> insert(value_type && key)
    {
        check_fullness();
        size_type insert_ind = find_index(key, true);
        bool inserted = false;
        if (!hash_set[insert_ind] || deleted[insert_ind]) {
            inserted = true;
            hash_set[insert_ind].template emplace(std::move(key));
            update_after_insert(insert_ind);
        }
        return std::make_pair(iterator(&hash_set, &deleted, insert_ind), inserted);
    }

    iterator insert(const_iterator /*hint*/, const value_type & key)
    {
        return insert(key).first;
    }
    iterator insert(const_iterator /*hint*/, value_type && key)
    {
        return insert(std::move(key)).first;
    }

    template <class InputIt>
    void insert(InputIt first, InputIt last)
    {
        for (auto it = first; it != last; ++it) {
            insert(*it);
        }
    }

    void insert(std::initializer_list<value_type> init)
    {
        insert(init.begin(), init.end());
    }

    // construct element in-place, no copy or move operations are performed;
    // element's constructor is called with exact same arguments as `emplace` method
    // (using `std::forward<Args>(args)...`)
    template <class... Args>
    std::pair<iterator, bool> emplace(Args &&... args)
    {
        key_type key(std::forward<Args>(args)...);
        if (load_factor() >= max_load_factor()) {
            rehash(set_max_size * 2);
        }
        size_type insert_ind = find_index(key, true);
        bool inserted = false;
        if (!hash_set[insert_ind] || deleted[insert_ind]) {
            inserted = true;
            hash_set[insert_ind].template emplace(std::move(key));
            update_after_insert(insert_ind);
        }
        return std::make_pair(iterator(&hash_set, &deleted, insert_ind), inserted);
    }

    template <class... Args>
    iterator emplace_hint(const_iterator /*hint*/, Args &&... args)
    {
        return emplace(std::forward<Args>(args)...).first;
    }

    iterator erase(const_iterator pos)
    {
        auto it = iterator(&hash_set, &deleted, pos.current);
        if (hash_set[pos.current]) {
            deleted[pos.current] = true;
            --set_size;
            ++it;
            if (pos.current == first_ind) {
                first_ind = it.current;
            }
        }
        return it;
    }

    iterator erase(const_iterator first, const_iterator last)
    {
        const_iterator it = first;
        for (; it != last; ++it) {
            erase(it);
        }
        return iterator(&hash_set, &deleted, it.current);
    }
    size_type erase(const key_type & key)
    {
        const_iterator it = find(key);
        if (it != cend()) {
            erase(it);
            return 1;
        }
        else {
            return 0;
        }
    }

    // exchanges the contents of the container with those of other;
    // does not invoke any move, copy, or swap operations on individual elements
    void swap(HashSet & other) noexcept
    {
        std::swap(hash_set, other.hash_set);
        std::swap(deleted, other.deleted);
        std::swap(set_size, other.set_size);
        std::swap(set_max_size, other.set_max_size);
        std::swap(first_ind, other.first_ind);
    }

    size_type count(const key_type & key) const
    {
        iterator it = find(key);
        if (it == end()) {
            return 0;
        }
        else {
            return 1;
        }
    }
    iterator find(const key_type & key)
    {
        size_type find_ind = find_index(key, false);
        if (find_ind == set_max_size * 2) {
            return end();
        }
        else {
            return iterator(&hash_set, &deleted, find_ind);
        }
    }

    const_iterator find(const key_type & key) const
    {
        size_type find_ind = find_index(key, false);
        if (find_ind == set_max_size * 2) {
            return cend();
        }
        else {
            return const_iterator(&hash_set, &deleted, find_ind);
        }
    }

    bool contains(const key_type & key) const
    {
        return find(key) != end();
    }
    std::pair<iterator, iterator> equal_range(const key_type & key)
    {
        iterator it = find(key);
        if (it != end()) {
            auto temp = it;
            ++it;
            return std::make_pair(temp, it);
        }
        else {
            return std::make_pair(it, it);
        }
    }
    std::pair<const_iterator, const_iterator> equal_range(const key_type & key) const
    {
        const_iterator it = find(key);
        if (it != cend()) {
            auto temp = it;
            ++it;
            return std::make_pair(temp, it);
        }
        else {
            return std::make_pair(it, it);
        }
    }

    size_type bucket_count() const
    {
        return set_max_size * 2;
    }

    size_type max_bucket_count() const
    {
        return set_max_size * 2;
    }

    size_type bucket_size(const size_type) const
    {
        return 1;
    }

    size_type bucket(const key_type & key) const
    {
        return set_hasher(key) % hash_set.size();
    }

    float load_factor() const
    {

        return max_size() ? size() / static_cast<float>(bucket_count()) : 1;
    }

    float max_load_factor() const
    {
        return 0.5;
    }
    void rehash(const size_type count)
    {
        size_type new_size;
        if (count == 0 && hash_set.empty()) {
            new_size = 2;
        }
        else if (count < size() * 2) {
            new_size = size() * 2 + 2;
        }
        else {
            new_size = count + 1;
        }
        std::vector<std::optional<value_type>> temp_set(new_size * 2);
        std::vector<bool> temp_deleted(new_size * 2, true);
        std::swap(hash_set, temp_set);
        std::swap(deleted, temp_deleted);
        set_max_size = new_size;
        set_size = 0;
        first_ind = new_size * 2;
        for (size_type i = 0; i < temp_set.size(); ++i) {
            if (temp_set[i] && !temp_deleted[i]) {
                emplace(std::move(*temp_set[i]));
            }
        }
    }
    void reserve(size_type count)
    {
        if (count > size() * 2 + 2) {
            rehash(count);
        }
    }

    // compare two containers contents
    friend bool operator==(const HashSet & lhs, const HashSet & rhs)
    {
        if (lhs.size() != rhs.size()) {
            return false;
        }
        for (const auto & element : lhs) {
            if (!rhs.contains(element)) {
                return false;
            }
        }
        return true;
    }
    friend bool operator!=(const HashSet & lhs, const HashSet & rhs)
    {
        return !(lhs == rhs);
    }

private:
    size_type set_size = 0;
    size_type set_max_size;
    std::vector<std::optional<value_type>> hash_set;
    std::vector<bool> deleted;
    const hasher set_hasher;
    const key_equal set_equal;
    size_type first_ind = set_max_size * 2;

    size_type find_index(const key_type & key, bool insert) const
    {
        if (hash_set.empty() && !insert) {
            return set_max_size * 2;
        }
        size_type current_ind = set_hasher(key) % hash_set.size();
        CollisionPolicy policy = CollisionPolicy(current_ind, hash_set.size());
        while (hash_set[current_ind] && (!deleted[current_ind] || !insert) && !set_equal(*hash_set[current_ind], key)) {
            current_ind = ++policy;
        }
        return (insert || (hash_set[current_ind] && !deleted[current_ind] && set_equal(*hash_set[current_ind], key))) ? current_ind : set_max_size * 2;
    }

    void update_after_insert(size_type to_update)
    {
        deleted[to_update] = false;
        first_ind = empty() ? to_update : std::min(first_ind, to_update);
        ++set_size;
    }

    void check_fullness()
    {
        if (load_factor() >= max_load_factor()) {
            rehash(set_max_size * 2);
        }
    }

public:
    class SetIterator
    {
        friend HashSet;
        using set = const std::vector<std::optional<value_type>> *;

        using status_vector = const std::vector<bool> *;

        set this_set;
        status_vector deleted;
        size_type current;

        using traits = std::iterator_traits<const value_type *>;

        SetIterator(set this_set, status_vector deleted, size_type current)
            : this_set(this_set)
            , deleted(deleted)
            , current(current)
        {
        }

    public:
        SetIterator() = default;

        SetIterator(const const_iterator & iterator)
            : SetIterator(iterator.this_set, iterator.deleted, iterator.current)
        {
        }

        using value_type = typename traits::value_type;
        using iterator_category = std::forward_iterator_tag;
        using difference_type = std::ptrdiff_t;
        using pointer = typename traits::pointer;
        using reference = typename traits::reference;

        SetIterator & operator++()
        {
            do {
                ++current;
            } while (current < this_set->size() && (!((*this_set)[current]) || (*deleted)[current]));
            return *this;
        }

        SetIterator operator++(int)
        {
            SetIterator temp = *this;
            do {
                ++current;
            } while (current < this_set->size() && (!((*this_set)[current]) || (*deleted)[current]));
            return temp;
        }

        reference operator*() const
        {
            return *((*this_set)[current]);
        }

        pointer operator->() const
        {
            return &(*((*this_set)[current]));
        }

        bool operator==(const SetIterator & iterator2) const
        {
            return this_set == iterator2.this_set && current == iterator2.current;
        }

        bool operator!=(const SetIterator & iterator2) const
        {
            return !(*this == iterator2);
        }

        SetIterator & operator=(const SetIterator & iterator) = default;
    };
};