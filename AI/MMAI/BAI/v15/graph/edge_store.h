#include <boost/multi_index/hashed_index.hpp>
#include <boost/multi_index/identity.hpp>
#include <boost/multi_index/indexed_by.hpp>
#include <boost/multi_index/random_access_index.hpp>
#include <boost/multi_index_container.hpp>

#include <ranges>

namespace detail
{
    struct by_src {};
    struct by_dst {};

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
            boost::multi_index::random_access<>,

            boost::multi_index::hashed_unique<
                boost::multi_index::identity<std::shared_ptr<T>>
            >,
            boost::multi_index::hashed_non_unique<
                boost::multi_index::tag<by_src>,
                SrcKey<T>
            >,
            boost::multi_index::hashed_non_unique<
                boost::multi_index::tag<by_dst>,
                DstKey<T>
            >
        >
    >;
}

template <typename ElemType>
class EdgeStore
{
public:
    using ElemPtr = std::shared_ptr<ElemType>;

    EdgeStore() = default;

    EdgeStore(const EdgeStore &) = delete;
    EdgeStore & operator=(const EdgeStore &) = delete;
    EdgeStore(EdgeStore &&) = delete;
    EdgeStore & operator=(EdgeStore &&) = delete;

    // "Perfect" forwarding function which reserves lvalue/rvalue category:
    // lvalues are copied, rvalues are moved.
    // Convenient for using .add() with plain rvalue objects because they
    // have better compile-time support and type hints.
    template <typename T>
        requires std::same_as<std::remove_cvref_t<T>, ElemType>
    void add(T&& elem)
    {
        container.push_back(std::make_shared<ElemType>(std::forward<T>(elem)));
    }

    std::shared_ptr<const ElemType> getById(std::size_t ind) const
    {
        const auto& idx = container.template get<0>();
        if (ind >= idx.size())
            return nullptr;
        return idx[ind];
    }

    auto entries() const
    {
        const auto & idx = container.template get<0>();
        return entriesFrom(idx.begin(), idx.end());
    }

    std::size_t size() const
    {
        return container.size();
    }

    auto bySrc(const typename ElemType::src_node_type & src) const
    {
        const auto & idx = container.template get<detail::by_src>();
        auto [first, last] = idx.equal_range(&src);
        return entriesFrom(first, last);
    }

    auto byDst(const typename ElemType::dst_node_type & dst) const
    {
        const auto& idx = container.template get<detail::by_dst>();
        auto [first, last] = idx.equal_range(dst);
        return entriesFrom(first, last);
    }

private:
    detail::MultiIndexEdgeContainer<ElemType> container;

    auto entriesFrom(auto first, auto last) const
    {
        return std::ranges::subrange(first, last)
            | std::views::transform(
                [](const ElemPtr& ptr) -> const ElemType& {
                    return *ptr;
                }
            );
    }
};
