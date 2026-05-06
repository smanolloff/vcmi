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

    template <typename EdgeType>
    struct SrcKey
    {
        using result_type = std::shared_ptr<const typename EdgeType::src_node_type>;
        result_type operator()(const std::shared_ptr<EdgeType> & edge) const
        {
            return edge->srcNode;
        }
    };

    template <typename EdgeType>
    struct DstKey
    {
        using result_type = std::shared_ptr<const typename EdgeType::dst_node_type>;
        result_type operator()(const std::shared_ptr<EdgeType> & edge) const
        {
            return edge->dstNode;
        }
    };


    template <typename T>
    using MultiIndexEdgeContainer = boost::multi_index::multi_index_container<
        std::shared_ptr<const T>,
        boost::multi_index::indexed_by<
            boost::multi_index::random_access<
                boost::multi_index::tag<by_ordinal_id>
            >,

            boost::multi_index::hashed_unique<
                boost::multi_index::tag<by_ptr_identity>,
                boost::multi_index::identity<std::shared_ptr<const T>>
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

    // XXX: pass-by-value + move is preferred, see comment in NodeStore::add
    void add(std::shared_ptr<EdgeType> edge)
    {
        // Insertion fails when there is a duplicate in *any* unique index
        auto [_, inserted] = container.push_back(std::move(edge));
        if(!inserted)
            throw std::runtime_error(std::string(EdgeType::encoding_traits::name) + ": add: insertion failed. Duplicate index?");
    }

    std::shared_ptr<const EdgeType> getById(std::size_t ind) const
    {
        const auto & idx = container.template get<detail::by_ordinal_id>();
        if (ind >= idx.size())
            throw std::runtime_error(std::string(EdgeType::encoding_traits::name) + ": getById: not found: " + std::to_string(ind));
        return idx[ind];
    }

    std::shared_ptr<const EdgeType> getByIdentity(const std::shared_ptr<const EdgeType> & edge) const
    {
        const auto & idx = container.template get<detail::by_ptr_identity>();
        auto it = idx.find(edge);
        if (it == idx.end())
            throw std::runtime_error(std::string(EdgeType::encoding_traits::name) + ": getByIdentity: not found");
        return *it;
    }

    const auto & entries() const
    {
        return container.template get<detail::by_ordinal_id>();
    }

    auto getAllBySrc(const std::shared_ptr<const typename EdgeType::src_node_type> & src) const
    {
        const auto & idx = container.template get<detail::by_src_node>();
        auto [first, last] = idx.equal_range(src);
        return std::ranges::subrange(first, last);
    }

    auto getAllByDst(const std::shared_ptr<const typename EdgeType::dst_node_type> & dst) const
    {
        const auto & idx = container.template get<detail::by_dst_node>();
        auto [first, last] = idx.equal_range(dst);
        return std::ranges::subrange(first, last);
    }

    std::size_t size() const
    {
        return container.size();
    }

    std::ptrdiff_t getId(const std::shared_ptr<const EdgeType> & edge) const
    {
        const auto & identity_idx = container.template get<detail::by_ptr_identity>();

        auto identity_it = identity_idx.find(edge);
        if (identity_it == identity_idx.end())
            throw std::runtime_error(std::string(EdgeType::encoding_traits::name) + ": getId: not found");

        const auto & ordinal_idx = container.template get<detail::by_ordinal_id>();
        auto ordinal_it = container.template project<detail::by_ordinal_id>(identity_it);
        return std::distance(ordinal_idx.begin(), ordinal_it);
    }

private:
    detail::MultiIndexEdgeContainer<EdgeType> container;
};
