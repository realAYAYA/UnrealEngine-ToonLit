"""64 bit FNV-1a hash module"""
from typing import List, Optional, Any


class FNV1a:
    """
    64 Bit FNV-1a Hashing Algorithm:

    http://www.isthe.com/chongo/tech/comp/fnv/index.html#FNV-1a

    s/n = seed/offset/hashed value
    t = text string
    c = character in string
    p = 64 bit prime (2**40 + 2**8 + 0xb3)
    m = 64 bit bitmask
    h = final 64 bit hash value

    HASH:
    t = ( c_1 + c_2 + ... + c_x )
    n_1 = s
    n_2 = ( n_1 ^ c_1 ) * p
    n_x = ( n_(x-1) ^ c_(x-1) ) * p
    h = n_x & m

    DEHASH FROM HASH LIST:
    c_x = ( n_x // p ) ^ n_(x-1)
    c_2 = ( n_2 // p ) ^ n_1
    c_1 = ( n_1 // p ) ^ s
    t = ( c_1 + c_2 + ... + c_x )
    ...

    USAGE:
    >>> hasher = FNV1a()
    >>> hasher
    FNV1a(seed=14695981039346656037,
          prime=1099511628211,
          mask=18446744073709551615,
          text=None,
          hash_out=None,
          hash_list=[])
    >>>
    >>> hasher.hash("This is a test.com bro.")
    'ade2f4095d74bf44'
    >>>
    >>> new_hasher = FNV1a()
    >>> new_hasher
    FNV1a(seed=14695981039346656037,
          prime=1099511628211,
          mask=18446744073709551615,
          text=None,
          hash_out=None,
          hash_list=[])
    >>>
    >>> new_hasher.dehash(hasher.hash_list)
    'This is a test.com bro.'
    >>>
    """
    _seed: int = 14695981039346656037
    _prime: int = 1099511628211
    _mask: int = 0xFFFFFFFFFFFFFFFF

    def __init__(self) -> None:
        self.hash_list: List[int] = []
        self.hash_out: Optional[str] = None
        self.text: Optional[str] = None

    def __repr__(self) -> str:
        return (f"{self.__class__.__name__}(seed={self._seed},\n"
                f"      prime={self._prime},\n"
                f"      mask={self._mask},\n"
                f"      text={self.text},\n"
                f"      hash_out={self.hash_out},\n"
                f"      hash_list={self.hash_list})")

    def hash(self, text: Optional[str]) -> Optional[str]:
        """Creates a 64 bit hash from a string input.

        Parameters
        ----------
        arg1 : str
            Text to hash

        Returns
        -------
        str
            64 bit fvn-1a hash of parameter string
        """
        n_x, prime, mask = self._seed, self._prime, self._mask
        self._clear()
        self.text = text
        hash_list = self.hash_list
        if self.text:
            for char in self.text:
                n_x = (n_x ^ ord(char)) * prime
                hash_list.append(n_x)
            self.hash_out = hex(n_x & mask)[2:]
            hash_list.append(self._seed)
        return self.hash_out

    def _clear(self) -> None:
        """Clear variables from instance."""
        self.hash_list = []
        self.hash_out = None
        self.text = None

    def _type_check(self, value: Any, value_type: Any) -> None:
        """Check hash_list types"""
        if not isinstance(value, value_type):
            self._clear()
            raise TypeError("Must supply a list of integers")

    def dehash(self, hash_list: Optional[List[int]] = None) -> Optional[str]:
        """Dehashes/reverts a hash list used to build a hash and returns the
        original string.

        Parameters
        ----------
        arg1 : list of str
            List of hash states to "dehash"

        Returns
        -------
        str
            Original text that was hashed with list of hash states
        """
        prime, out = self._prime, []
        self._type_check(hash_list, list)
        if not hash_list:
            if not self.hash_list:
                self._clear()
                return ""
            hash_list = self.hash_list
        self._type_check(hash_list[0], int)
        try:
            for i in range(len(hash_list) - 1)[::-1]:
                self._type_check(i, int)
                char = (hash_list[i] // prime) ^ hash_list[i - 1]
                out.append(chr(char))
                self.text = "".join(out[::-1])
        except (OverflowError, ValueError) as error:
            self._clear()
            raise ValueError("Invalid input, could not be dehashed") from error
        self.hash_out, self.hash_list = self.hash(self.text), hash_list
        return self.text
