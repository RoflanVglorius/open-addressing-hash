#pragma once

#include "policy.h"

#include <memory>
#include <optional>
#include <stdexcept>
#include <vector>

template <
        class Key,
        class T,
        class CollisionPolicy = LinearProbing,
        class Hash = std::hash<Key>,
        class Equal = std::equal_to<Key>>
class HashMap
{

public:
    template <bool IsConst>
    class CommonIterator;
    // types
    using key_type = Key;
    using mapped_type = T;
    using value_type = std::pair<const Key, T>;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;
    using hasher = Hash;
    using key_equal = Equal;
    using reference = value_type &;
    using const_reference = const value_type &;
    using pointer = value_type *;
    using const_pointer = const value_type *;

    using iterator = CommonIterator<false>;
    using const_iterator = CommonIterator<true>;

    explicit HashMap(size_type expected_max_size = 0,
                     const hasher & hash = hasher(),
                     const key_equal & equal = key_equal())
        : map_max_size(expected_max_size)
        , hash_map(expected_max_size * 2)
        , deleted(expected_max_size * 2, true)
        , map_hasher(hash)
        , map_equal(equal)
    {
    }

    template <class InputIt>
    HashMap(InputIt first,
            InputIt last,
            size_type expected_max_size = 0,
            const hasher & hash = hasher(),
            const key_equal & equal = key_equal())
        : HashMap(expected_max_size, hash, equal)
    {
        for (auto it = first; it != last; ++it) {
            insert(*it);
        }
    }

    HashMap(const HashMap &) = default;

    HashMap(HashMap &&) = default;

    HashMap(std::initializer_list<value_type> init,
            size_type expected_max_size = 0,
            const hasher & hash = hasher(),
            const key_equal & equal = key_equal())
        : HashMap(init.begin(), init.end(), expected_max_size, hash, equal)
    {
    }

    ~HashMap() = default;

    HashMap & operator=(const HashMap & other)
    {
        return *this = HashMap(other);
    }

    HashMap & operator=(HashMap && new_map) noexcept
    {
        swap(new_map);
        return *this;
    }
    HashMap & operator=(std::initializer_list<value_type> init)
    {
        return *this = HashMap(init, init.size());
    }
    iterator begin() noexcept
    {
        return iterator(&hash_map, &deleted, first_ind);
    }
    const_iterator begin() const noexcept
    {
        return cbegin();
    }
    const_iterator cbegin() const noexcept
    {
        return const_iterator(&hash_map, &deleted, first_ind);
    }

    iterator end() noexcept
    {
        return iterator(&hash_map, &deleted, map_max_size * 2);
    }
    const_iterator end() const noexcept
    {
        return cend();
    }
    const_iterator cend() const noexcept
    {
        return const_iterator(&hash_map, &deleted, map_max_size * 2);
    }

    bool empty() const
    {
        return map_size == 0;
    }

    size_type size() const
    {
        return map_size;
    }

    size_type max_size() const
    {
        return map_max_size;
    }

    void clear()
    {
        hash_map.clear();
        deleted.clear();
        std::vector<std::optional<value_type>> temp_map(map_max_size * 2);
        std::vector<bool> temp_deleted(map_max_size * 2, true);
        std::swap(hash_map, temp_map);
        std::swap(deleted, temp_deleted);
        map_size = 0;
        first_ind = 0;
    }

    std::pair<iterator, bool> insert(const value_type & value)
    {
        return emplace(value);
    }

    std::pair<iterator, bool> insert(value_type && value)
    {
        return emplace(std::move(value));
    }

    template <class P>
    std::pair<iterator, bool> insert(P && value)
    {
        return emplace(std::forward<P>(value));
    }
    iterator insert(const_iterator /*hint*/, const value_type & value)
    {
        return insert(value).first;
    }
    iterator insert(const_iterator /*hint*/, value_type && value)
    {
        return insert(std::move(value)).first;
    }
    template <class P>
    iterator insert(const_iterator /*hint*/, P && value)
    {
        return insert(std::forward<P>(value)).first;
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

    template <class M>
    std::pair<iterator, bool> insert_or_assign(const key_type & key, M && value)
    {
        auto pair = emplace(key, std::forward<M>(value));
        if (!pair.second) {
            hash_map[pair.first.current]->second = value;
        }
        return pair;
    }
    template <class M>
    std::pair<iterator, bool> insert_or_assign(key_type && key, M && value)
    {
        auto pair = emplace(std::move(key), std::forward<M>(value));
        if (!pair.second) {
            hash_map[pair.first.current]->second = std::forward<M>(value);
        }
        return pair;
    }
    template <class M>
    iterator insert_or_assign(const_iterator /*hint*/, const key_type & key, M && value)
    {
        return insert_or_assign(key, std::forward<M>(value)).first;
    }
    template <class M>
    iterator insert_or_assign(const_iterator /*hint*/, key_type && key, M && value)
    {
        return insert_or_assign(std::move(key), std::forward<M>(value)).first;
    }

    // construct element in-place, no copy or move operations are performed;
    // element's constructor is called with exact same arguments as `emplace` method
    // (using `std::forward<Args>(args)...`)
    template <class... Args>
    std::pair<iterator, bool> emplace(Args &&... args)
    {
        return emplace_impl(std::forward<Args>(args)...);
    }

    template <class... Args>
    iterator emplace_hint(const_iterator /*hint*/, Args &&... args)
    {
        return emplace(std::forward<Args>(args)...).first;
    }

    template <class... Args>
    std::pair<iterator, bool> try_emplace(const key_type & key, Args &&... args)
    {
        check_fullness();
        size_type insert_ind = find_index(key, true);
        bool inserted = false;
        if (!hash_map[insert_ind] || deleted[insert_ind]) {
            inserted = true;
            hash_map[insert_ind].template emplace(std::piecewise_construct,
                                                  std::forward_as_tuple(key),
                                                  std::forward_as_tuple(std::forward<Args>(args)...));
            update_after_insert(insert_ind);
        }
        return std::make_pair(iterator(&hash_map, &deleted, insert_ind), inserted);
    }
    template <class... Args>
    std::pair<iterator, bool> try_emplace(key_type && key, Args &&... args)
    {
        check_fullness();
        size_type insert_ind = find_index(key, true);
        bool inserted = false;
        if (!hash_map[insert_ind] || deleted[insert_ind]) {
            inserted = true;
            hash_map[insert_ind].template emplace(std::piecewise_construct,
                                                  std::forward_as_tuple(std::move(key)),
                                                  std::forward_as_tuple(std::forward<Args>(args)...));
            update_after_insert(insert_ind);
        }
        return std::make_pair(iterator(&hash_map, &deleted, insert_ind), inserted);
    }
    template <class... Args>
    iterator try_emplace(const_iterator /*hint*/, const key_type & key, Args &&... args)
    {
        return try_emplace(key, std::forward<Args>(args)...).first;
    }
    template <class... Args>
    iterator try_emplace(const_iterator /*hint*/, key_type && key, Args &&... args)
    {
        return try_emplace(std::move(key), std::forward<Args>(args)...).first;
    }

    iterator erase(const_iterator pos)
    {
        auto it = iterator(&hash_map, &deleted, pos.current);
        if (hash_map[pos.current]) {
            deleted[pos.current] = true;
            --map_size;
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
        return iterator(&hash_map, &deleted, it.current);
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
    void swap(HashMap & other) noexcept
    {
        std::swap(hash_map, other.hash_map);
        std::swap(deleted, other.deleted);
        std::swap(map_size, other.map_size);
        std::swap(map_max_size, other.map_max_size);
        std::swap(first_ind, other.first_ind);
    }

    size_type count(const key_type & key) const
    {
        iterator it = find(key);
        if (it == cend()) {
            return 0;
        }
        else {
            return 1;
        }
    }

    iterator find(const key_type & key)
    {
        size_type find_ind = find_index(key, false);
        if (find_ind == map_max_size * 2) {
            return end();
        }
        else {
            return iterator(&hash_map, &deleted, find_ind);
        }
    }
    const_iterator find(const key_type & key) const
    {
        size_type find_ind = find_index(key, false);
        if (find_ind == map_max_size * 2) {
            return cend();
        }
        else {
            return const_iterator(&hash_map, &deleted, find_ind);
        }
    }

    bool contains(const key_type & key) const
    {
        return find(key) != cend();
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

    mapped_type & at(const key_type & key)
    {
        iterator it = find(key);
        if (it != end()) {
            return it->second;
        }
        else {
            throw std::out_of_range("No element with such key");
        }
    }
    const mapped_type & at(const key_type & key) const
    {
        const_iterator it = find(key);
        if (it != cend()) {
            return it->second;
        }
        else {
            throw std::out_of_range("No element with such key");
        }
    }

    mapped_type & operator[](const key_type & key)
    {
        return try_emplace(key).first->second;
    }

    mapped_type & operator[](key_type && key)
    {
        return try_emplace(std::move(key)).first->second;
    }

    size_type bucket_count() const
    {
        return map_max_size * 2;
    }

    size_type max_bucket_count() const
    {
        return map_max_size * 2;
    }

    size_type bucket_size(const size_type) const
    {
        return 1;
    }

    size_type bucket(const key_type & key) const
    {
        return map_hasher(key) % hash_map.size();
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
        if (count == 0 && hash_map.empty()) {
            new_size = 2;
        }
        else if (count < size() * 2) {
            new_size = size() * 2 + 2;
        }
        else {
            new_size = count + 1;
        }
        std::vector<std::optional<value_type>> temp_map(new_size * 2);
        std::vector<bool> temp_deleted(new_size * 2, true);
        std::swap(hash_map, temp_map);
        std::swap(deleted, temp_deleted);
        map_max_size = new_size;
        map_size = 0;
        first_ind = new_size * 2;
        for (size_type i = 0; i < temp_map.size(); ++i) {
            if (temp_map[i] && !temp_deleted[i]) {
                emplace(std::move(const_cast<key_type &>(temp_map[i]->first)), std::move(temp_map[i]->second));
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
    friend bool operator==(const HashMap & lhs, const HashMap & rhs)
    {
        if (lhs.size() != rhs.size()) {
            return false;
        }
        for (const auto & element : lhs) {
            if (!rhs.contains(element.first)) {
                return false;
            }
        }
        return true;
    }

    friend bool operator!=(const HashMap & lhs, const HashMap & rhs)
    {
        return !(lhs == rhs);
    }

private:
    size_type map_size = 0;
    size_type map_max_size;
    std::vector<std::optional<value_type>> hash_map;
    std::vector<bool> deleted;
    const hasher map_hasher;
    const key_equal map_equal;
    size_type first_ind = map_max_size * 2;

    size_type find_index(const key_type & key, bool insert) const
    {
        if (hash_map.empty() && !insert) {
            return map_max_size * 2;
        }
        size_type current_ind = map_hasher(key) % hash_map.size();
        CollisionPolicy policy = CollisionPolicy(current_ind, hash_map.size());
        while (hash_map[current_ind] && (!deleted[current_ind] || !insert) && !map_equal(hash_map[current_ind]->first, key)) {
            current_ind = ++policy;
        }
        return (insert || (hash_map[current_ind] && !deleted[current_ind] && map_equal(hash_map[current_ind]->first, key))) ? current_ind : map_max_size * 2;
    }

    template <class K, class V>
    std::pair<iterator, bool> emplace_impl(K && key, V && value)
    {
        check_fullness();
        size_type insert_ind = find_index(key, true);
        bool inserted = false;
        if (!hash_map[insert_ind] || deleted[insert_ind]) {
            inserted = true;
            hash_map[insert_ind].template emplace(std::forward<K>(key), std::forward<V>(value));
            update_after_insert(insert_ind);
        }
        return std::make_pair(iterator(&hash_map, &deleted, insert_ind), inserted);
    }

    template <class... KArgs, class... VArgs>
    std::pair<iterator, bool> emplace_impl(std::piecewise_construct_t, std::tuple<KArgs...> && to_key, std::tuple<VArgs...> && to_value)
    {
        check_fullness();
        key_type key(std::make_from_tuple<key_type>(std::forward<std::tuple<KArgs...>>(to_key)));
        size_type insert_ind = find_index(key, true);
        bool inserted = false;
        if (!hash_map[insert_ind] || deleted[insert_ind]) {
            inserted = true;
            hash_map[insert_ind].template emplace(
                    std::move(key),
                    std::make_from_tuple<mapped_type>(std::forward<std::tuple<VArgs...>>(to_value)));
            update_after_insert(insert_ind);
        }
        return std::make_pair(iterator(&hash_map, &deleted, insert_ind), inserted);
    }

    template <class P>
    std::pair<iterator, bool> emplace_impl(P && value)
    {
        check_fullness();
        size_type insert_ind = find_index(value.first, true);
        bool inserted = false;
        if (!hash_map[insert_ind] || deleted[insert_ind]) {
            inserted = true;
            hash_map[insert_ind].template emplace(std::forward<P>(value));
            update_after_insert(insert_ind);
        }
        return std::make_pair(iterator(&hash_map, &deleted, insert_ind), inserted);
    }

    void update_after_insert(size_type to_update)
    {
        deleted[to_update] = false;
        first_ind = empty() ? to_update : std::min(first_ind, to_update);
        ++map_size;
    }

    void check_fullness()
    {
        if (load_factor() >= max_load_factor()) {
            rehash(map_max_size * 2);
        }
    }

public:
    template <bool IsConst>
    class CommonIterator
    {
        friend HashMap;
        using map = std::conditional_t<IsConst,
                                       const std::vector<std::optional<value_type>> *,
                                       std::vector<std::optional<value_type>> *>;

        using status_vector = std::conditional_t<IsConst,
                                                 const std::vector<bool> *,
                                                 std::vector<bool> *>;

        map this_map;
        status_vector deleted;
        size_type current;

        using traits = std::iterator_traits<std::conditional_t<IsConst, const value_type *, value_type *>>;

        CommonIterator(map this_map, status_vector deleted, size_type current)
            : this_map(this_map)
            , deleted(deleted)
            , current(current)
        {
        }

    public:
        CommonIterator() = default;

        CommonIterator(const const_iterator & iterator)
            : CommonIterator(iterator.this_map, iterator.deleted, iterator.current)
        {
        }

        CommonIterator(const iterator & iterator)
            : CommonIterator(iterator.this_map, iterator.deleted, iterator.current)
        {
        }

        using value_type = typename traits::value_type;
        using iterator_category = std::forward_iterator_tag;
        using difference_type = std::ptrdiff_t;
        using pointer = typename traits::pointer;
        using reference = typename traits::reference;

        CommonIterator & operator++()
        {
            do {
                ++current;
            } while (current < this_map->size() && (!((*this_map)[current]) || (*deleted)[current]));
            return *this;
        }

        CommonIterator operator++(int)
        {
            CommonIterator temp = *this;
            do {
                ++current;
            } while (current < this_map->size() && (!((*this_map)[current]) || (*deleted)[current]));
            return temp;
        }

        reference operator*() const
        {
            return *((*this_map)[current]);
        }

        pointer operator->() const
        {
            return &(*((*this_map)[current]));
        }

        bool operator==(const CommonIterator & iterator2) const
        {
            return this_map == iterator2.this_map && current == iterator2.current;
        }

        bool operator!=(const CommonIterator & iterator2) const
        {
            return !(*this == iterator2);
        }

        CommonIterator & operator=(const CommonIterator & iterator) = default;
    };
};