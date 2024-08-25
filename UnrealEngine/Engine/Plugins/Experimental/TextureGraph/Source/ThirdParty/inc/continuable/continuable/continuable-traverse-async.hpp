
/*

                        /~` _  _ _|_. _     _ |_ | _
                        \_,(_)| | | || ||_|(_||_)|(/_

                    https://github.com/Naios/continuable
                                   v4.2.0

  Copyright(c) 2015 - 2022 Denis Blank <denis.blank at outlook dot com>

  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files(the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and / or sell
  copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions :

  The above copyright notice and this permission notice shall be included in
  all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
  SOFTWARE.
**/

#ifndef CONTINUABLE_TRAVERSE_ASYNC_HPP_INCLUDED
#define CONTINUABLE_TRAVERSE_ASYNC_HPP_INCLUDED

#include <utility>
#include <continuable/detail/traversal/traverse-async.hpp>

namespace cti {
/// \defgroup Traversal Traversal
/// provides functions to traverse and remap nested packs.
/// \{

/// A tag which is passed to the `operator()` of the visitor
/// if an element is visited synchronously through \ref traverse_pack_async.
///
/// \since 3.0.0
using async_traverse_visit_tag = detail::traversal::async_traverse_visit_tag;
/// A tag which is passed to the `operator()` of the visitor if an element is
/// visited after the traversal was detached through \ref traverse_pack_async.
///
/// \since 3.0.0
using async_traverse_detach_tag = detail::traversal::async_traverse_detach_tag;
/// A tag which is passed to the `operator()` of the visitor if the
/// asynchronous pack traversal was finished through \ref traverse_pack_async.
///
/// \since 3.0.0
using async_traverse_complete_tag =
    detail::traversal::async_traverse_complete_tag;

/// A tag to identify that a mapper shall be constructed in-place
/// from the first argument passed to \ref traverse_pack_async.
///
/// \since 3.0.0
template <typename T>
using async_traverse_in_place_tag =
    detail::traversal::async_traverse_in_place_tag<T>;

/// Traverses the pack with the given visitor in an asynchronous way.
///
/// This function works in the same way as `traverse_pack`,
/// however, we are able to suspend and continue the traversal at
/// later time.
/// Thus we require a visitor callable object which provides three
/// `operator()` overloads as depicted by the code sample below:
///    ```cpp
///    struct my_async_visitor {
///      /// The synchronous overload is called for each object,
///      /// it may return false to suspend the current control flow.
///      /// In that case the overload below is called.
///      template <typename T>
///      bool operator()(async_traverse_visit_tag, T&& element) {
///        return true;
///      }
///
///      /// The asynchronous overload this is called when the
///      /// synchronous overload returned false.
///      /// In addition to the current visited element the overload is
///      /// called with a contnuation callable object which resumes the
///      /// traversal when it's called later.
///      /// The continuation next may be stored and called later or
///      /// dropped completely to abort the traversal early.
///      template <typename T, typename N>
///      void operator()(async_traverse_detach_tag, T&& element, N&& next) {
///      }
///
///      /// The overload is called when the traversal was finished.
///      /// As argument the whole pack is passed over which we
///      /// traversed asynchrnously.
///      template <typename T>
///      void operator()(async_traverse_complete_tag, T&& pack) {
///      }
///    };
///    ```
///
/// \param   visitor A visitor object which provides the three `operator()`
///                  overloads that were described above.
///                  Additionally the visitor must be compatible
///                  for referencing it from a `boost::intrusive_ptr`.
///                  The visitor should must have a virtual destructor!
///
/// \param   pack    The arbitrary parameter pack which is traversed
///                  asynchronously. Nested objects inside containers and
///                  tuple like types are traversed recursively.
///
/// \returns         A std::shared_ptr that references an instance of
///                  the given visitor object.
///
/// \since           3.0.0
///
/// See `traverse_pack` for a detailed description about the
/// traversal behaviour and capabilities.
///
template <typename Visitor, typename... T>
auto traverse_pack_async(Visitor&& visitor, T&&... pack) {
  return detail::traversal::apply_pack_transform_async(
      std::forward<Visitor>(visitor), std::forward<T>(pack)...);
}
/// \}
} // namespace cti

#endif // CONTINUABLE_TRAVERSE_ASYNC_HPP_INCLUDED
