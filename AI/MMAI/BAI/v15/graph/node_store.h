#include <boost/multi_index/hashed_index.hpp>
#include <boost/multi_index/identity.hpp>
#include <boost/multi_index/indexed_by.hpp>
#include <boost/multi_index/random_access_index.hpp>
#include <boost/multi_index_container.hpp>

#include <memory>
#include <ranges>
#include <stdexcept>

namespace detail
{
    // Tags for boost multi_index
    // This allows to access them by name
    // (only random_access index is accessed by index: 0)
    struct by_ordinal_id;
    struct by_ptr_identity;
    struct by_extra_index;

    template <typename NodeType>
    struct node_ptr_index {
        using result_type = const NodeType*;
        const NodeType* operator()(const std::shared_ptr<NodeType>& ptr) const {
            return ptr.get();
        }
    };

    // Helper template with partial specialization
    template <typename T>
    struct MultiIndexContainerHelper;

    // Specialization without extra index (e.g. Global nodes)
    template <typename T>
        requires std::is_same_v<typename T::extra_index_type, void>
    struct MultiIndexContainerHelper<T> {
        using type = boost::multi_index::multi_index_container<
            std::shared_ptr<T>,
            boost::multi_index::indexed_by<
                boost::multi_index::random_access<
                    boost::multi_index::tag<by_ordinal_id>
                >,
                boost::multi_index::hashed_unique<
                    boost::multi_index::tag<by_ptr_identity>,
                    node_ptr_index<T>
                >
            >
        >;
    };

    // Specialization with extra index
    // To define an extra index, declare a block in the class's public section:
    //
    //     struct extra_index_type {
    //         using result_type = int16_t;
    //         result_type operator()(const std::shared_ptr<Hex> & hex) const {
    //             return hex->bhex.toInt();
    //         }
    //     };
    template <typename T>
        requires (!std::is_same_v<typename T::extra_index_type, void>)
    struct MultiIndexContainerHelper<T> {
        using type = boost::multi_index::multi_index_container<
            std::shared_ptr<T>,
            boost::multi_index::indexed_by<
                boost::multi_index::random_access<
                    boost::multi_index::tag<by_ordinal_id>
                >,

                boost::multi_index::hashed_unique<
                    boost::multi_index::tag<by_ptr_identity>,
                    node_ptr_index<T>
                >,

                boost::multi_index::hashed_unique<
                    boost::multi_index::tag<by_extra_index>,
                    typename T::extra_index_type
                >
            >
        >;
    };

    // Then expose the alias
    template <typename T>
    using MultiIndexNodeContainer = typename MultiIndexContainerHelper<T>::type;
}

template <typename NodeType>
class NodeStore
{
public:
    NodeStore() = default;

    NodeStore(const NodeStore &) = delete;
    NodeStore & operator=(const NodeStore &) = delete;
    NodeStore(NodeStore &&) = delete;
    NodeStore & operator=(NodeStore &&) = delete;

    const NodeType & add(const std::shared_ptr<NodeType> & node)
    {
        // Insertion fails when there is a duplicate in *any* unique index
        auto [it, inserted] = container.push_back(node);
        if(!inserted)
            throw std::runtime_error(std::string(NodeType::encoding_traits::name) + ": insertion failed. Duplicate index?");
        return **it;
    }

    std::shared_ptr<const NodeType> getById(std::size_t ind) const
    {
        const auto& idx = container.template get<0>();
        if (ind >= idx.size())
            return nullptr;
        return idx[ind];
    }

    std::shared_ptr<const NodeType> getByIdentity(NodeType & node) const
    {
        const auto& idx = container.template get<detail::by_ptr_identity>();
        auto it = idx.find(&node);
        if (it == idx.end())
            return nullptr;
        return *it;
    }

    // This notation (with explicit typename Key) is preferrable as it allows
    // to pass "convertible" (or "compatible") types.
    // E.g. if the index type is std::string, then passing "foo" here is OK.
    // Without it, the argument would have to be exactly std::string("foo").
    template <typename Key>
        requires (!std::is_same_v<typename NodeType::extra_index_type, void>)
    std::shared_ptr<const NodeType> getByExtraIndex(const Key& key) const
    {
        const auto& idx = container.template get<detail::by_extra_index>();
        auto it = idx.find(key);
        if (it == idx.end())
            return nullptr;
        return *it;
    }

    auto entries() const
    {
        const auto & idx = container.template get<detail::by_ordinal_id>();

        // return std::ranges::subrange(idx.begin(), idx.end());
        return idx | std::views::transform(
            [](const std::shared_ptr<NodeType> & ptr) -> const NodeType & {
                return *ptr;
            }
        );
    }

    std::size_t size() const
    {
        return container.size();
    }

    std::ptrdiff_t getId(const NodeType & node) const
    {
        const auto& identity_idx = container.template get<detail::by_ptr_identity>();

        auto identity_it = identity_idx.find(&node);
        if (identity_it == identity_idx.end())
            throw std::runtime_error("getId: node not found");

        const auto& ordinal_idx = container.template get<detail::by_ordinal_id>();
        auto ordinal_it = container.template project<detail::by_ordinal_id>(identity_it);
        return std::distance(ordinal_idx.begin(), ordinal_it);
    }
private:
    detail::MultiIndexNodeContainer<NodeType> container;
};
