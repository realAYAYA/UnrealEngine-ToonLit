// Copyright Epic Games, Inc. All Rights Reserved.

#pragma autortfm

#include "API.h"
#include "Catch2Includes.h"

#include <AutoRTFM/AutoRTFM.h>
#include <memory>
#include <thread>

TEST_CASE("API.autortfm_result")
{
    int Answer = 6 * 9;

    REQUIRE(autortfm_committed == autortfm_transact([](void* const Arg)
    {
        *static_cast<int* const>(Arg) = 42;
    }, &Answer));

    REQUIRE(42 == Answer);

    REQUIRE(autortfm_aborted_by_request == autortfm_transact([](void* const Arg)
    {
        *static_cast<int* const>(Arg) = 13;
        AutoRTFM::AbortTransaction();
    }, &Answer));

    REQUIRE(42 == Answer);

    REQUIRE(autortfm_aborted_by_request == autortfm_transact([](void* const Arg)
    {
        *static_cast<int* const>(Arg) = 13;
        AutoRTFM::AbortIfTransactional();
    }, &Answer));

    REQUIRE(42 == Answer);
}

TEST_CASE("API.autortfm_is_transactional")
{
    REQUIRE(false == autortfm_is_transactional());

    bool InTransaction = false;
    bool InOpenNest = false;

    AutoRTFM::Commit([&]
    {
        InTransaction = autortfm_is_transactional();

        AutoRTFM::Open([&]
        {
            InOpenNest = autortfm_is_transactional();
        });
    });

    REQUIRE(true == InTransaction);
    REQUIRE(true == InOpenNest);
}

TEST_CASE("API.autortfm_is_closed")
{
    REQUIRE(false == autortfm_is_closed());

    // Set to the opposite of what we expect at the end of function.
    bool InTransaction = false;
    bool InOpenNest = true;
    bool InClosedNestInOpenNest = false;

    AutoRTFM::Commit([&]
    {
        InTransaction = autortfm_is_closed();

        AutoRTFM::Open([&]
        {
            InOpenNest = autortfm_is_closed();

            REQUIRE(AutoRTFM::EContextStatus::OnTrack == AutoRTFM::Close([&]
            {
                InClosedNestInOpenNest = autortfm_is_closed();
            }));
        });
    });

    REQUIRE(true == InTransaction);
    REQUIRE(false == InOpenNest);
    REQUIRE(true == InClosedNestInOpenNest);
}

TEST_CASE("API.autortfm_transact")
{
    int Answer = 6 * 9;

    REQUIRE(autortfm_committed == autortfm_transact([](void* const Arg)
    {
        *static_cast<int* const>(Arg) = 42;
    }, &Answer));

    REQUIRE(42 == Answer);
}

TEST_CASE("API.autortfm_commit")
{
    int Answer = 6 * 9;

    autortfm_commit([](void* const Arg)
    {
        *static_cast<int* const>(Arg) = 42;
    }, &Answer);

    REQUIRE(42 == Answer);
}

TEST_CASE("API.autortfm_abort_transaction")
{
    bool BeforeNest = false;
    bool InNest = false;
    bool AfterNest = false;
    AutoRTFM::ETransactionResult NestResult;

    AutoRTFM::Commit([&]
    {
        BeforeNest = true;

        NestResult = AutoRTFM::Transact([&]
        {
            // Because we are aborting this won't actually occur!
            InNest = true;

            autortfm_abort_transaction();
        });

        AfterNest = true;
    });

    REQUIRE(true == BeforeNest);
    REQUIRE(false == InNest);
    REQUIRE(true == AfterNest);
    REQUIRE(AutoRTFM::ETransactionResult::AbortedByRequest == NestResult);
}

TEST_CASE("API.autortfm_abort_if_transactional")
{
    // Calling this outwith any transaction won't abort the program.
    autortfm_abort_if_transactional();

    bool BeforeNest = false;
    bool InNest = false;
    bool AfterNest = false;
    AutoRTFM::ETransactionResult NestResult;

    AutoRTFM::Commit([&]
    {
        BeforeNest = true;

        NestResult = AutoRTFM::Transact([&]
        {
            // Because we are aborting this won't actually occur!
            InNest = true;

            autortfm_abort_if_transactional();
        });

        AfterNest = true;
    });

    REQUIRE(true == BeforeNest);
    REQUIRE(false == InNest);
    REQUIRE(true == AfterNest);
    REQUIRE(AutoRTFM::ETransactionResult::AbortedByRequest == NestResult);
}

TEST_CASE("API.autortfm_abort_if_closed")
{
    // Calling this outwith any transaction won't abort the program.
    autortfm_abort_if_closed();

    bool BeforeNest = false;
    bool InNest = false;
    bool AfterNest = false;

    REQUIRE(AutoRTFM::ETransactionResult::AbortedByRequest == AutoRTFM::Transact([&]
    {
        BeforeNest = true;

        AutoRTFM::Open([&]
        {
            InNest = true;

            // This won't abort because we aren't closed!
            autortfm_abort_if_closed();
        });

        AfterNest = true;

        autortfm_abort_if_closed();
    }));

    REQUIRE(false == BeforeNest);
    REQUIRE(true == InNest);
    REQUIRE(false == AfterNest);
}

TEST_CASE("API.autortfm_open")
{
    int Answer = 6 * 9;

    // An open call outside a transaction succeeds.
    autortfm_open([](void* const Arg)
    {
        *static_cast<int* const>(Arg) = 42;
    }, &Answer);

    REQUIRE(42 == Answer);

    REQUIRE(AutoRTFM::ETransactionResult::AbortedByRequest == AutoRTFM::Transact([&]
    {
        // An open call inside a transaction succeeds also.
        autortfm_open([](void* const Arg)
        {
            *static_cast<int* const>(Arg) *= 2;
        }, &Answer);

        AutoRTFM::AbortTransaction();
    }));

    REQUIRE(84 == Answer);
}

TEST_CASE("API.autortfm_close")
{
    bool InClosedNest = false;
    bool InOpenNest = false;
    bool InClosedNestInOpenNest = false;

    REQUIRE(AutoRTFM::ETransactionResult::AbortedByRequest == AutoRTFM::Transact([&]
    {
        // A closed call inside a transaction does not abort.
        REQUIRE(autortfm_status_ontrack == autortfm_close([](void* const Arg)
        {
            *static_cast<bool* const>(Arg) = true;
        }, &InClosedNest));

        AutoRTFM::Open([&]
        {
            // A closed call inside an open does not abort either.
			REQUIRE(autortfm_status_ontrack == autortfm_close([](void* const Arg)
            {
                *static_cast<bool* const>(Arg) = true;
            }, &InClosedNestInOpenNest));

            InOpenNest = true;
        });

        AutoRTFM::AbortTransaction();
    }));

    REQUIRE(false == InClosedNest);
    REQUIRE(true == InOpenNest);
    REQUIRE(false == InClosedNestInOpenNest);
}

TEST_CASE("API.autortfm_register_open_function")
{
    autortfm_register_open_function(
        reinterpret_cast<void*>(NoAutoRTFM::DoSomethingC),
        reinterpret_cast<void*>(NoAutoRTFM::DoSomethingInTransactionC));

    int I = -42;

    AutoRTFM::Commit([&]
    {
        I = NoAutoRTFM::DoSomethingC(I);
    });

    REQUIRE(0 == I);
}

TEST_CASE("API.autortfm_on_commit")
{
    bool OuterTransaction = false;
    bool InnerTransaction = false;
    bool InnerTransactionWithAbort = false;
    bool InnerOpenNest = false;
    AutoRTFM::ETransactionResult NestResult;

    AutoRTFM::Commit([&]
    {
        autortfm_on_commit([](void* const Arg)
        {
            *static_cast<bool* const>(Arg) = true;
        }, &OuterTransaction);

        // This should only be modified on the commit!
        if (OuterTransaction)
        {
            AutoRTFM::AbortTransaction();
        }

        AutoRTFM::Commit([&]
        {
            autortfm_on_commit([](void* const Arg)
            {
                *static_cast<bool* const>(Arg) = true;
            }, &InnerTransaction);
        });

        // This should only be modified on the commit!
        if (InnerTransaction)
        {
            AutoRTFM::AbortTransaction();
        }

        NestResult = AutoRTFM::Transact([&]
        {
            autortfm_on_commit([](void* const Arg)
            {
                *static_cast<bool* const>(Arg) = true;
            }, &InnerTransactionWithAbort);

            AutoRTFM::AbortTransaction();
        });

        // This should never be modified because its transaction aborted!
        if (InnerTransactionWithAbort)
        {
            AutoRTFM::AbortTransaction();
        }

        AutoRTFM::Open([&]
        {
            autortfm_on_commit([](void* const Arg)
            {
                *static_cast<bool* const>(Arg) = true;
            }, &InnerOpenNest);

            // This should be modified immediately!
            if (!InnerOpenNest)
            {
                AutoRTFM::AbortTransaction();
            }
        });
    });

    REQUIRE(true == OuterTransaction);
    REQUIRE(true == InnerTransaction);
    REQUIRE(false == InnerTransactionWithAbort);
    REQUIRE(true == InnerOpenNest);
    REQUIRE(AutoRTFM::ETransactionResult::AbortedByRequest == NestResult);
}

TEST_CASE("API.autortfm_on_abort")
{
    bool OuterTransaction = false;
    bool InnerTransaction = false;
    bool InnerTransactionWithAbort = false;
    bool InnerOpenNest = false;
    AutoRTFM::ETransactionResult NestResult;

    REQUIRE(AutoRTFM::ETransactionResult::Committed == AutoRTFM::Transact([&]
    {
        autortfm_on_abort([](void* const Arg)
        {
            *static_cast<bool* const>(Arg) = true;
        }, &OuterTransaction);

        // This should only be modified on the commit!
        if (OuterTransaction)
        {
            AutoRTFM::AbortTransaction();
        }

        AutoRTFM::Commit([&]
        {
            autortfm_on_abort([](void* const Arg)
            {
                *static_cast<bool* const>(Arg) = true;
            }, &InnerTransaction);
        });

        // This should only be modified on the commit!
        if (InnerTransaction)
        {
            AutoRTFM::AbortTransaction();
        }

        NestResult = AutoRTFM::Transact([&]
        {
            autortfm_on_abort([](void* const Arg)
            {
                *static_cast<bool* const>(Arg) = true;
            }, &InnerTransactionWithAbort);

            AutoRTFM::AbortTransaction();
        });

        // This should never be modified because its transaction aborted!
        if (InnerTransactionWithAbort)
        {
            AutoRTFM::AbortTransaction();
        }

        AutoRTFM::Open([&]
        {
            autortfm_on_abort([](void* const Arg)
            {
                *static_cast<bool* const>(Arg) = true;
            }, &InnerOpenNest);
        });

        // This should only be modified on the commit!
        if (InnerOpenNest)
        {
            AutoRTFM::AbortTransaction();
        }
    }));

    REQUIRE(false == OuterTransaction);
    REQUIRE(false == InnerTransaction);
    REQUIRE(true == InnerTransactionWithAbort);
    REQUIRE(false == InnerOpenNest);
    REQUIRE(AutoRTFM::ETransactionResult::AbortedByRequest == NestResult);
}

TEST_CASE("API.autortfm_did_allocate")
{
    constexpr unsigned Size = 1024;
    unsigned BumpAllocator[Size];
    unsigned NextBump = 0;

    AutoRTFM::Commit([&]
    {
        for (unsigned I = 0; I < Size; I++)
        {
            unsigned* Data;
            AutoRTFM::Open([&]
            {
                Data = reinterpret_cast<unsigned*>(autortfm_did_allocate(
                    &BumpAllocator[NextBump++],
                    sizeof(unsigned)));
            });

            *Data = I;
        }
    });

    for (unsigned I = 0; I < Size; I++)
    {
        REQUIRE(I == BumpAllocator[I]);
    }
}

TEST_CASE("API.autortfm_check_consistency_assuming_no_races")
{
    AutoRTFM::Commit([&]
    {
        autortfm_check_consistency_assuming_no_races();
    });
}

TEST_CASE("API.ETransactionResult")
{
    int Answer = 6 * 9;

    REQUIRE(AutoRTFM::ETransactionResult::Committed == AutoRTFM::Transact([&]
    {
        Answer = 42;
    }));

    REQUIRE(42 == Answer);

    REQUIRE(AutoRTFM::ETransactionResult::AbortedByRequest == AutoRTFM::Transact([&]
    {
        Answer = 13;
        AutoRTFM::AbortTransaction();
    }));

    REQUIRE(42 == Answer);

    REQUIRE(AutoRTFM::ETransactionResult::AbortedByRequest == AutoRTFM::Transact([&]
    {
        Answer = 13;
        AutoRTFM::AbortIfTransactional();
    }));
    
    REQUIRE(42 == Answer);
}

TEST_CASE("API.IsTransactional")
{
    REQUIRE(false == AutoRTFM::IsTransactional());

    bool InTransaction = false;
    bool InOpenNest = false;

    AutoRTFM::Commit([&]
    {
        InTransaction = AutoRTFM::IsTransactional();

        AutoRTFM::Open([&]
        {
            InOpenNest = AutoRTFM::IsTransactional();
        });
    });

    REQUIRE(true == InTransaction);
    REQUIRE(true == InOpenNest);
}

TEST_CASE("API.IsClosed")
{
    REQUIRE(false == AutoRTFM::IsClosed());

    // Set to the opposite of what we expect at the end of function.
    bool InTransaction = false;
    bool InOpenNest = true;
    bool InClosedNestInOpenNest = false;

    AutoRTFM::Commit([&]
    {
        InTransaction = AutoRTFM::IsClosed();

        AutoRTFM::Open([&]
        {
            InOpenNest = AutoRTFM::IsClosed();

            REQUIRE(AutoRTFM::EContextStatus::OnTrack == AutoRTFM::Close([&]
            {
                InClosedNestInOpenNest = AutoRTFM::IsClosed();
            }));
        });
    });

    REQUIRE(true == InTransaction);
    REQUIRE(false == InOpenNest);
    REQUIRE(true == InClosedNestInOpenNest);
}

TEST_CASE("API.Transact")
{
    int Answer = 6 * 9;

    REQUIRE(AutoRTFM::ETransactionResult::Committed == AutoRTFM::Transact([&]
    {
        Answer = 42;
    }));

    REQUIRE(42 == Answer);
}

TEST_CASE("API.Commit")
{
    int Answer = 6 * 9;

    AutoRTFM::Commit([&]
    {
        Answer = 42;
    });

    REQUIRE(42 == Answer);
}

TEST_CASE("API.Abort")
{
    bool BeforeNest = false;
    bool InNest = false;
    bool AfterNest = false;
    AutoRTFM::ETransactionResult NestResult;

    AutoRTFM::Commit([&]
    {
        BeforeNest = true;

        NestResult = AutoRTFM::Transact([&]
        {
            // Because we are aborting this won't actually occur!
            InNest = true;

            AutoRTFM::AbortTransaction();
        });

        AfterNest = true;
    });

    REQUIRE(true == BeforeNest);
    REQUIRE(false == InNest);
    REQUIRE(true == AfterNest);
    REQUIRE(AutoRTFM::ETransactionResult::AbortedByRequest == NestResult);
}

TEST_CASE("API.AbortIfTransactional")
{
    // Calling this outwith any transaction won't abort the program.
    AutoRTFM::AbortIfTransactional();

    bool BeforeNest = false;
    bool InNest = false;
    bool AfterNest = false;
    AutoRTFM::ETransactionResult NestResult;

    AutoRTFM::Commit([&]
    {
        BeforeNest = true;

        NestResult = AutoRTFM::Transact([&]
        {
            // Because we are aborting this won't actually occur!
            InNest = true;

            AutoRTFM::AbortIfTransactional();
        });

        AfterNest = true;
    });

    REQUIRE(true == BeforeNest);
    REQUIRE(false == InNest);
    REQUIRE(true == AfterNest);
    REQUIRE(AutoRTFM::ETransactionResult::AbortedByRequest == NestResult);
}

TEST_CASE("API.AbortIfClosed")
{
    // Calling this outwith any transaction won't abort the program.
    AutoRTFM::AbortIfClosed();

    bool BeforeNest = false;
    bool InNest = false;
    bool AfterNest = false;

    REQUIRE(AutoRTFM::ETransactionResult::AbortedByRequest == AutoRTFM::Transact([&]
    {
        BeforeNest = true;

        AutoRTFM::Open([&]
        {
            InNest = true;

            // This won't abort because we aren't closed!
            AutoRTFM::AbortIfClosed();
        });

        AfterNest = true;

        AutoRTFM::AbortIfClosed();
    }));

    REQUIRE(false == BeforeNest);
    REQUIRE(true == InNest);
    REQUIRE(false == AfterNest);
}

TEST_CASE("API.Open")
{
    int Answer = 6 * 9;

    // An open call outside a transaction succeeds.
    AutoRTFM::Open([&]
    {
        Answer = 42;
    });

    REQUIRE(42 == Answer);

    REQUIRE(AutoRTFM::ETransactionResult::AbortedByRequest == AutoRTFM::Transact([&]
    {
        // An open call inside a transaction succeeds also.
        AutoRTFM::Open([&]
        {
            Answer *= 2;
        });

        AutoRTFM::AbortTransaction();
    }));

    REQUIRE(84 == Answer);
}

TEST_CASE("API.Close")
{
    bool InClosedNest = false;
    bool InOpenNest = false;
    bool InClosedNestInOpenNest = false;

    REQUIRE(AutoRTFM::ETransactionResult::AbortedByRequest == AutoRTFM::Transact([&]
    {
        // A closed call inside a transaction does not abort.
        REQUIRE(AutoRTFM::EContextStatus::OnTrack == AutoRTFM::Close([&]
        {
            InClosedNest = true;
        }));

        AutoRTFM::Open([&]
        {
            // A closed call inside an open does not abort either.
			REQUIRE(AutoRTFM::EContextStatus::OnTrack == AutoRTFM::Close([&]
            {
                InClosedNestInOpenNest = true;
            }));

            InOpenNest = true;
        });

        AutoRTFM::AbortTransaction();
    }));

    REQUIRE(false == InClosedNest);
    REQUIRE(true == InOpenNest);
    REQUIRE(false == InClosedNestInOpenNest);
}

TEST_CASE("API.RegisterOpenFunction")
{
    AutoRTFM::ForTheRuntime::RegisterOpenFunction(
        reinterpret_cast<void*>(NoAutoRTFM::DoSomethingCpp),
        reinterpret_cast<void*>(NoAutoRTFM::DoSomethingInTransactionCpp));

    int I = -42;

    AutoRTFM::Commit([&]
    {
        I = NoAutoRTFM::DoSomethingCpp(I);
    });

    REQUIRE(0 == I);
}

TEST_CASE("API.OnCommit")
{
    bool OuterTransaction = false;
    bool InnerTransaction = false;
    bool InnerTransactionWithAbort = false;
    bool InnerOpenNest = false;
    AutoRTFM::ETransactionResult NestResult;

    REQUIRE(AutoRTFM::ETransactionResult::Committed == AutoRTFM::Transact([&]
    {
        AutoRTFM::OnCommit([&]
        {
            OuterTransaction = true;
        });

        // This should only be modified on the commit!
        if (OuterTransaction)
        {
            AutoRTFM::AbortTransaction();
        }

        AutoRTFM::Commit([&]
        {
            AutoRTFM::OnCommit([&]
            {
                InnerTransaction = true;
            });
        });

        // This should only be modified on the commit!
        if (InnerTransaction)
        {
			AutoRTFM::AbortTransaction();
        }

        NestResult = AutoRTFM::Transact([&]
        {
            AutoRTFM::OnCommit([&]
            {
                InnerTransactionWithAbort = true;
            });

		AutoRTFM::AbortTransaction();
        });

        // This should never be modified because its transaction aborted!
        if (InnerTransactionWithAbort)
        {
			AutoRTFM::AbortTransaction();
        }

        AutoRTFM::Open([&]
        {
            AutoRTFM::OnCommit([&]
            {
                InnerOpenNest = true;
            });

            // This should be modified immediately!
            if (!InnerOpenNest)
            {
				AutoRTFM::AbortTransaction();
            }
        });
    }));

    REQUIRE(true == OuterTransaction);
    REQUIRE(true == InnerTransaction);
    REQUIRE(false == InnerTransactionWithAbort);
    REQUIRE(true == InnerOpenNest);
    REQUIRE(AutoRTFM::ETransactionResult::AbortedByRequest == NestResult);
}

TEST_CASE("API.OnAbort")
{
    bool OuterTransaction = false;
    bool InnerTransaction = false;
    bool InnerTransactionWithAbort = false;
    bool InnerOpenNest = false;
    AutoRTFM::ETransactionResult NestResult;

    REQUIRE(AutoRTFM::ETransactionResult::Committed == AutoRTFM::Transact([&]
    {
        AutoRTFM::OnAbort([&]
        {
            OuterTransaction = true;
        });

        // This should only be modified on the commit!
        if (OuterTransaction)
        {
			AutoRTFM::AbortTransaction();
        }

        AutoRTFM::Commit([&]
        {
            AutoRTFM::OnAbort([&]
            {
                InnerTransaction = true;
            });
        });

        // This should only be modified on the commit!
        if (InnerTransaction)
        {
			AutoRTFM::AbortTransaction();
        }

        NestResult = AutoRTFM::Transact([&]
        {
            AutoRTFM::OnAbort([&]
            {
                InnerTransactionWithAbort = true;
            });

		AutoRTFM::AbortTransaction();
        });

        // This should never be modified because its transaction aborted!
        if (InnerTransactionWithAbort)
        {
			AutoRTFM::AbortTransaction();
        }

        AutoRTFM::Open([&]
        {
            AutoRTFM::OnAbort([&]
            {
                InnerOpenNest = true;
            });
        });

        // This should only be modified on the commit!
        if (InnerOpenNest)
        {
			AutoRTFM::AbortTransaction();
        }
    }));

    REQUIRE(false == OuterTransaction);
    REQUIRE(false == InnerTransaction);
    REQUIRE(true == InnerTransactionWithAbort);
    REQUIRE(false == InnerOpenNest);
    REQUIRE(AutoRTFM::ETransactionResult::AbortedByRequest == NestResult);
}

TEST_CASE("API.DidAllocate")
{
    constexpr unsigned Size = 1024;
    unsigned BumpAllocator[Size];
    unsigned NextBump = 0;

    AutoRTFM::Commit([&]
    {
        for (unsigned I = 0; I < Size; I++)
        {
            unsigned* Data;
            AutoRTFM::Open([&]
            {
                Data = reinterpret_cast<unsigned*>(AutoRTFM::DidAllocate(
                    &BumpAllocator[NextBump++],
                    sizeof(unsigned)));
            });

            *Data = I;
        }
    });

    for (unsigned I = 0; I < Size; I++)
    {
        REQUIRE(I == BumpAllocator[I]);
    }
}

TEST_CASE("API.CheckConsistencyAssumingNoRaces")
{
    AutoRTFM::Commit([&]
    {
        AutoRTFM::ForTheRuntime::CheckConsistencyAssumingNoRaces();
    });
}
