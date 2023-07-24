/*
 * Copyright 1995, 1996 Perforce Software.  All rights reserved.
 *
 * This file is part of Perforce - the FAST SCM System.
 */

/*
 * Signaler.h - catch ^C and delete temp files
 *
 * A single Signaler is declared globally.
 *
 * Public methods:
 *
 *	Signaler::Block() -- don't catch the signal until Catch()
 *	Signaler::Catch() -- catch and handle SIGINT
 *	Signaler::OnIntr() -- call a designated function on SIGINT
 *	Signaler::DeleteOnIntr() -- undo OnIntr() call
 *
 *	Signaler::Intr() -- call functions registered by OnIntr()
 *
 * Requires cooperation from the TempFile objects to delete files.
 */

# ifdef OS_NT
typedef void *HANDLE;
# else // OS_NT
# ifdef HAS_CPP11
# include <memory>
# include <mutex>
# endif // HAS_CPP11
# endif // OS_NT

struct SignalMan;

typedef void (*SignalFunc)( void *ptr );

class Signaler {

    public:
			Signaler();
			~Signaler();
	void		Init();

	void		Block();
	void		Catch();
	void		Disable();
	void		Enable();
	bool		GetState() const;
	bool		IsIntr() const;

	void		OnIntr( SignalFunc callback, void *ptr );
	void		DeleteOnIntr( void *ptr );

	void		Intr();

    private:

	SignalMan	*list;
	int		disable;
	bool		isIntr;

	// If we're compiling with the C++11 standard or higher, we use
	// the built-in thread support on all platforms.  If not, we fall
	// back to only having synchronization on Windows.

# ifdef OS_NT
	HANDLE		hmutex;
# else // OS_NT
# ifdef HAS_CPP11
		std::mutex* mutex;

		std::mutex&	GetMutex();
# endif // HAS_CPP11
# endif // OS_NT

} ;

extern Signaler signaler;
