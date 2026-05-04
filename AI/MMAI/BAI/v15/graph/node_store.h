#include <boost/multi_index/hashed_index.hpp>
#include <boost/multi_index/identity.hpp>
#include <boost/multi_index/indexed_by.hpp>
#include <boost/multi_index/random_access_index.hpp>
#include <boost/multi_index_container.hpp>

#include <ranges>

template <typename T>
using MultiIndexContainer = boost::multi_index::multi_index_container<
    std::shared_ptr<T>,
    boost::multi_index::indexed_by<
        boost::multi_index::random_access<>,
        boost::multi_index::hashed_unique<
            boost::multi_index::identity<std::shared_ptr<T>>
        >
    >
>;

template <typename ElemType>
class NodeStore
{
public:
    using ElemPtr = std::shared_ptr<ElemType>;

    NodeStore() = default;

    NodeStore(const NodeStore &) = delete;
    NodeStore & operator=(const NodeStore &) = delete;
    NodeStore(NodeStore &&) = delete;
    NodeStore & operator=(NodeStore &&) = delete;

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

    std::shared_ptr<const ElemType> get(std::size_t ind) const
    {
        const auto& idx = container.template get<0>();
        if (ind >= idx.size())
            return nullptr;
        return idx[ind];
    }

    auto entries() const
    {
        const auto & idx = container.template get<0>();

        // return std::ranges::subrange(idx.begin(), idx.end());
        return idx | std::views::transform(
            [](const ElemPtr & ptr) -> const ElemType & {
                return *ptr;
            }
        );
    }

    std::size_t size() const
    {
        return container.size();
    }

private:
    MultiIndexContainer<ElemType> container;
};
