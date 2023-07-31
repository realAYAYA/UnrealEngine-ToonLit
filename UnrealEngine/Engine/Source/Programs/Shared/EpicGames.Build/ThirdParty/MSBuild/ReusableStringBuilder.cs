// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

using System;
using System.Collections.Concurrent;
using System.Collections.Generic;
using System.Diagnostics.CodeAnalysis;
using System.Globalization;
using System.Text;
using System.Threading;

//EPIC BEGIN
#nullable disable
//EPIC END

namespace Microsoft.Build.Shared
{
    /// <summary>
    /// A StringBuilder lookalike that reuses its internal storage.
    /// </summary>
    /// <remarks>
    /// This class is being deprecated in favor of SpanBasedStringBuilder in StringTools. Avoid adding more uses.
    /// </remarks>
    internal sealed class ReuseableStringBuilder : IDisposable
    {
        /// <summary>
        /// Captured string builder.
        /// </summary>
        private StringBuilder _borrowedBuilder;

        /// <summary>
        /// Capacity to initialize the builder with.
        /// </summary>
        private int _capacity;

        /// <summary>
        /// Create a new builder, under the covers wrapping a reused one.
        /// </summary>
        internal ReuseableStringBuilder(int capacity = 16) // StringBuilder default is 16
        {
            _capacity = capacity;

            // lazy initialization of the builder
        }

        /// <summary>
        /// The length of the target.
        /// </summary>
        public int Length
        {
            get { return (_borrowedBuilder == null) ? 0 : _borrowedBuilder.Length; }
            set
            {
                LazyPrepare();
                _borrowedBuilder.Length = value;
            }
        }

        /// <summary>
        /// Convert to a string.
        /// </summary>
        public override string ToString()
        {
            if (_borrowedBuilder == null)
            {
                return String.Empty;
            }

            return _borrowedBuilder.ToString();
        }

        /// <summary>
        /// Dispose, indicating you are done with this builder.
        /// </summary>
        void IDisposable.Dispose()
        {
            if (_borrowedBuilder != null)
            {
                ReuseableStringBuilderFactory.Release(_borrowedBuilder);
                _borrowedBuilder = null;
                _capacity = -1;
            }
        }

        /// <summary>
        /// Append a character.
        /// </summary>
        internal ReuseableStringBuilder Append(char value)
        {
            LazyPrepare();
            _borrowedBuilder.Append(value);
            return this;
        }

        /// <summary>
        /// Append a string.
        /// </summary>
        internal ReuseableStringBuilder Append(string value)
        {
            LazyPrepare();
            _borrowedBuilder.Append(value);
            return this;
        }

        /// <summary>
        /// Append a substring.
        /// </summary>
        internal ReuseableStringBuilder Append(string value, int startIndex, int count)
        {
            LazyPrepare();
            _borrowedBuilder.Append(value, startIndex, count);
            return this;
        }

        public ReuseableStringBuilder AppendSeparated(char separator, ICollection<string> strings)
        {
            LazyPrepare();

            var separatorsRemaining = strings.Count - 1;

            foreach (var s in strings)
            {
                _borrowedBuilder.Append(s);

                if (separatorsRemaining > 0)
                {
                    _borrowedBuilder.Append(separator);
                }

                separatorsRemaining--;
            }

            return this;
        }

        public ReuseableStringBuilder Clear()
        {
            LazyPrepare();
            _borrowedBuilder.Clear();
            return this;
        }

        /// <summary>
        /// Remove a substring.
        /// </summary>
        internal ReuseableStringBuilder Remove(int startIndex, int length)
        {
            LazyPrepare();
            _borrowedBuilder.Remove(startIndex, length);
            return this;
        }

        /// <summary>
        /// Grab a backing builder if necessary.
        /// </summary>
        private void LazyPrepare()
        {
            if (_borrowedBuilder == null)
            {
                ErrorUtilities.VerifyThrow(_capacity != -1, "Reusing after dispose");

                _borrowedBuilder = ReuseableStringBuilderFactory.Get(_capacity);
            }
        }

        /// <summary>
        /// A utility class that mediates access to a shared string builder.
        /// </summary>
        /// <remarks>
        /// If this shared builder is highly contended, this class could add
        /// a second one and try both in turn.
        /// </remarks>
        private static class ReuseableStringBuilderFactory
        {
            /// <summary>
            /// Made up limit beyond which we won't share the builder
            /// because we could otherwise hold a huge builder indefinitely.
            /// This size seems reasonable for MSBuild uses (mostly expression expansion)
            /// </summary>
            private const int MaxBuilderSize = 1024;

            /// <summary>
            /// The shared builder.
            /// </summary>
            private static StringBuilder s_sharedBuilder;

#if DEBUG
            /// <summary>
            /// Count of successful reuses
            /// </summary>
            private static int s_hits = 0;

            /// <summary>
            /// Count of failed reuses - a new builder was created
            /// </summary>
            private static int s_misses = 0;

            /// <summary>
            /// Count of times the builder capacity was raised to satisfy the caller's request
            /// </summary>
            private static int s_upsizes = 0;

            /// <summary>
            /// Count of times the returned builder was discarded because it was too large
            /// </summary>
            private static int s_discards = 0;

            /// <summary>
            /// Count of times the builder was returned.
            /// </summary>
            private static int s_accepts = 0;

            /// <summary>
            /// Aggregate capacity saved (aggregate midpoints of requested and returned)
            /// </summary>
            private static int s_saved = 0;

            /// <summary>
            /// Callstacks of those handed out and not returned yet
            /// </summary>
            private static ConcurrentDictionary<StringBuilder, string> s_handouts = new ConcurrentDictionary<StringBuilder, string>();
#endif
            /// <summary>
            /// Obtains a string builder which may or may not already
            /// have been used. 
            /// Never returns null.
            /// </summary>
            internal static StringBuilder Get(int capacity)
            {
#if DEBUG
                bool missed = false;
#endif
                var returned = Interlocked.Exchange(ref s_sharedBuilder, null);

                if (returned == null)
                {
#if DEBUG
                    missed = true;
                    Interlocked.Increment(ref s_misses);
#endif
                    // Currently loaned out so return a new one
                    returned = new StringBuilder(capacity);
                }
                else if (returned.Capacity < capacity)
                {
#if DEBUG
                    Interlocked.Increment(ref s_upsizes);
#endif
                    // It's essential we guarantee the capacity because this
                    // may be used as a buffer to a PInvoke call.
                    returned.Capacity = capacity;
                }

#if DEBUG
                Interlocked.Increment(ref s_hits);

                if (!missed)
                {
                    Interlocked.Add(ref s_saved, (capacity + returned.Capacity) / 2);
                }

                // handouts.TryAdd(returned, Environment.StackTrace);
#endif
                return returned;
            }

            /// <summary>
            /// Returns the shared builder for the next caller to use.
            /// ** CALLERS, DO NOT USE THE BUILDER AFTER RELEASING IT HERE! **
            /// </summary>
            internal static void Release(StringBuilder returningBuilder)
            {
                // It's possible for someone to cause the builder to
                // enlarge to such an extent that this static field
                // would be a leak. To avoid that, only accept
                // the builder if it's no more than a certain size.
                //
                // If some code has a bug and forgets to return their builder
                // (or we refuse it here because it's too big) the next user will
                // get given a new one, and then return it soon after. 
                // So the shared builder will be "replaced".
                if (returningBuilder.Capacity < MaxBuilderSize)
                {
                    // ErrorUtilities.VerifyThrow(handouts.TryRemove(returningBuilder, out dummy), "returned but not loaned");
                    returningBuilder.Clear(); // Clear before pooling

                    Interlocked.Exchange(ref s_sharedBuilder, returningBuilder);
#if DEBUG
                    Interlocked.Increment(ref s_accepts);
                }
                else
                {
                    Interlocked.Increment(ref s_discards);
#endif
                }
            }

#if DEBUG
            /// <summary>
            /// Debugging dumping
            /// </summary>
            [SuppressMessage("Microsoft.Performance", "CA1811:AvoidUncalledPrivateCode", Justification = "Handy helper method that can be used to annotate ReuseableStringBuilder when debugging it, but is not hooked up usually for the sake of perf.")]
            [SuppressMessage("Microsoft.Usage", "CA1806:DoNotIgnoreMethodResults", MessageId = "System.String.Format(System.IFormatProvider,System.String,System.Object[])", Justification = "Handy string that can be used to annotate ReuseableStringBuilder when debugging it, but is not hooked up usually.")]
            internal static void DumpUnreturned()
            {
                String.Format(CultureInfo.CurrentUICulture, "{0} Hits of which\n    {1} Misses (was on loan)\n    {2} Upsizes (needed bigger) \n\n{3} Returns=\n{4}    Discards (returned too large)+\n    {5} Accepts\n\n{6} estimated bytes saved", s_hits, s_misses, s_upsizes, s_discards + s_accepts, s_discards, s_accepts, s_saved);

                Console.WriteLine("Unreturned string builders were allocated here:");
                foreach (var entry in s_handouts.Values)
                {
                    Console.WriteLine(entry + "\n");
                }
            }
#endif
        }
    }
}
