#include <boost/multi_index/hashed_index.hpp>
#include <boost/multi_index/identity.hpp>
#include <boost/multi_index/indexed_by.hpp>
#include <boost/multi_index/random_access_index.hpp>
#include <boost/multi_index_container.hpp>

#include <ranges>

namespace detail
{
    struct by_ordinal_id;
    struct by_ptr_identity;
    struct by_src_node;
    struct by_dst_node;

    template <typename Edge>
    struct SrcKey
    {
        using result_type = const typename Edge::src_node_type*;

        result_type operator()(const std::shared_ptr<Edge>& edge) const
        {
            return &edge->srcNode;
        }
    };

    template <typename Edge>
    struct DstKey
    {
        using result_type = const typename Edge::dst_node_type*;

        result_type operator()(const std::shared_ptr<Edge>& edge) const
        {
            return &edge->dstNode;
        }
    };

    template <typename T>
    using MultiIndexEdgeContainer = boost::multi_index::multi_index_container<
        std::shared_ptr<T>,
        boost::multi_index::indexed_by<
            boost::multi_index::random_access<
                boost::multi_index::tag<by_ordinal_id>
            >,

            boost::multi_index::hashed_unique<
                boost::multi_index::tag<by_ptr_identity>,
                boost::multi_index::identity<std::shared_ptr<T>>
            >,
            boost::multi_index::hashed_non_unique<
                boost::multi_index::tag<by_src_node>,
                SrcKey<T>
            >,
            boost::multi_index::hashed_non_unique<
                boost::multi_index::tag<by_dst_node>,
                DstKey<T>
            >
        >
    >;
}

template <typename EdgeType>
class EdgeStore
{
public:
    EdgeStore() = default;

    EdgeStore(const EdgeStore &) = delete;
    EdgeStore & operator=(const EdgeStore &) = delete;
    EdgeStore(EdgeStore &&) = delete;
    EdgeStore & operator=(EdgeStore &&) = delete;

    const EdgeType & add(const std::shared_ptr<EdgeType> edge)
    {
        // Insertion fails when there is a duplicate in *any* unique index
        auto [it, inserted] = container.push_back(edge);
        if(!inserted)
            throw std::runtime_error(std::string(EdgeType::encoding_traits::name) + ": insertion failed. Duplicate index?");
        return **it;
    }

    std::shared_ptr<const EdgeType> getById(std::size_t ind) const
    {
        const auto& idx = container.template get<detail::by_ptr_identity>();
        if (ind >= idx.size())
            return nullptr;
        return idx[ind];
    }

    std::shared_ptr<const EdgeType> getByIdentity(EdgeType & edge) const
    {
        const auto& idx = container.template get<detail::by_ptr_identity>();
        auto it = idx.find(&edge);
        if (it == idx.end())
            return nullptr;
        return *it;
    }

    auto entries() const
    {
        const auto & idx = container.template get<detail::by_ptr_identity>();
        // XXX: can't use entriesFrom here as the resulting range
        // is not detected as const for some reason
        return idx | std::views::transform(
            [](const std::shared_ptr<EdgeType> & ptr) -> const EdgeType & {
                return *ptr;
            }
        );
    }

    auto getAllBySrc(const typename EdgeType::src_node_type & src) const
    {
        const auto & idx = container.template get<detail::by_src_node>();
        auto [first, last] = idx.equal_range(&src);
        return entriesFrom(first, last);
    }

    auto getAllByDst(const typename EdgeType::dst_node_type & dst) const
    {
        const auto& idx = container.template get<detail::by_dst_node>();
        auto [first, last] = idx.equal_range(dst);
        return entriesFrom(first, last);
    }

    std::size_t size() const
    {
        return container.size();
    }

    std::ptrdiff_t getId(const EdgeType & edge) const
    {
        const auto& identity_idx = container.template get<detail::by_ptr_identity>();

        auto identity_it = identity_idx.find(&edge);
        if (identity_it == identity_idx.end())
            throw std::runtime_error("getId: edge not found");

        const auto& ordinal_idx = container.template get<detail::by_ordinal_id>();
        auto ordinal_it = container.template project<detail::by_ordinal_id>(identity_it);
        return std::distance(ordinal_idx.begin(), ordinal_it);
    }

private:
    detail::MultiIndexEdgeContainer<EdgeType> container;

    auto entriesFrom(auto first, auto last) const
    {
        return std::ranges::subrange(first, last)
            | std::views::transform(
                [](const std::shared_ptr<EdgeType>& ptr) -> const EdgeType& {
                    return *ptr;
                }
            );
    }
};
