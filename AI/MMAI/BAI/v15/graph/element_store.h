#include <boost/multi_index/hashed_index.hpp>
#include <boost/multi_index/identity.hpp>
#include <boost/multi_index/indexed_by.hpp>
#include <boost/multi_index/random_access_index.hpp>
#include <boost/multi_index_container.hpp>

#include <ranges>

template <typename T>
using MultiIndexContainer = boost::multi_index::multi_index_container<
    T,
    boost::multi_index::indexed_by<
        boost::multi_index::random_access<>,
        boost::multi_index::hashed_unique<
            boost::multi_index::identity<T>
        >
    >
>;

template <typename ElemType>
class ElementStore
{
public:
    ElementStore() = default;

    ElementStore(const ElementStore &) = delete;
    ElementStore & operator=(const ElementStore &) = delete;
    ElementStore(ElementStore &&) = delete;
    ElementStore & operator=(ElementStore &&) = delete;

    void add(ElemType elem)
    {
        container.push_back(std::move(elem));
    }

    const ElemType& get(std::size_t ind) const
    {
        auto ptr = container.template get<0>().at(ind);

        if (!ptr)
        {
            throw std::runtime_error(
                "Null element in store for element type: " +
                std::to_string(EU(ElemType::encoding_traits::element_type))
            );
        }

        return *ptr;
    }

    std::ranges::subrange<
        typename MultiIndexContainer<ElemType>::template nth_index<0>::type::const_iterator
    >
    entries() const
    {
        const auto& idx = container.template get<0>();
        return {idx.begin(), idx.end()};
    }

private:
    MultiIndexContainer<ElemType> container;
};
