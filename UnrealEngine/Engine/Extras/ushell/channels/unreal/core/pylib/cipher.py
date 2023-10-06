# Copyright Epic Games, Inc. All Rights Reserved.

import secrets
from pathlib import Path
from typing import Tuple, List, Iterator

#-------------------------------------------------------------------------------
def _rol(x:int, n:int) -> int:
    x &= 0xffffffff
    x = (x << n) | (x >> (32 - n))
    return x

#-------------------------------------------------------------------------------
def _qr(x:List[int], a:int, b:int, c:int, d:int) -> None:
    x[a] += x[b]; x[d] = _rol(x[d] ^ x[a], 16)
    x[c] += x[d]; x[b] = _rol(x[b] ^ x[c], 12)
    x[a] += x[b]; x[d] = _rol(x[d] ^ x[a],  8)
    x[c] += x[d]; x[b] = _rol(x[b] ^ x[c],  7)

#-------------------------------------------------------------------------------
def _read_blocks(key:bytes, nonce:bytes, counter:int=0) -> Iterator[bytes]:
    key = memoryview(key).cast("L")
    nonce = memoryview(nonce).cast("L")
    state = [0x61707865, 0x3320646e, 0x79622d32, 0x6b206574, *key, 0, 0, *nonce]
    out = memoryview(bytearray(4 * 16)).cast("L")
    while True:
        state[12] = counter
        state[13] = counter >> 32
        counter += 1

        block = list(state)

        for i in range(10):
            _qr(block, 0, 4,  8, 12); _qr(block, 1, 5,  9, 13)
            _qr(block, 2, 6, 10, 14); _qr(block, 3, 7, 11, 15)
            _qr(block, 0, 5, 10, 15); _qr(block, 1, 6, 11, 12)
            _qr(block, 2, 7,  8, 13); _qr(block, 3, 4,  9, 14)

        for i in range(len(block)):
            out[i] = (block[i] + state[i]) & 0xffffffff

        yield from (x for x in out.cast("B"))

#-------------------------------------------------------------------------------
def _chacha20(data:bytes, *, key:memoryview, nonce:memoryview):
    assert len(key) == 32
    assert len(nonce) == 8
    return bytes(a ^ b for a, b in zip(data, _read_blocks(key, nonce)))

#-------------------------------------------------------------------------------
def _hash(x):
    ret = 5381
    for c in x:
        ret = ((ret * 33) + c) & 0xffffffff
    return ret



#-------------------------------------------------------------------------------
class Blob(object):
    _data : memoryview
    _lede : memoryview
    _toc  : memoryview

    def __init__(self, data:memoryview|None=None) -> None:
        if not data:
            data = bytearray(secrets.token_bytes(32 << 10))
            data = memoryview(data)
        self._set_data(data)
        self._ranges = []
        self._toc_set = [0] * (len(self._toc) // 128)

    def _set_data(self, data):
        self._data = data.cast("B")
        self._lede = data[:256]
        self._toc = data[256:4096]

    def get_data(self) -> memoryview:
        return self._data

    def _diffuse(self, key:memoryview) -> memoryview:
        index = key[0] % (len(self._lede) - 48)
        data = _chacha20(self._lede[index:index + 48], key=key, nonce=b'\0' * 8)
        data = memoryview(data)
        return data[:32], data[32:40], data[40:48]

    def find(self, key:memoryview) -> Iterator[Tuple[str, memoryview]]:
        key, nonce, nonce_toc = self._diffuse(key)
        for i in range(0, len(self._toc), 128):
            piece = self._toc[i:i + 128]
            piece = _chacha20(piece, key=key, nonce=nonce_toc)
            piece = memoryview(piece).cast("L")
            name = piece[4:].cast("B")
            if piece[2] == _hash(name):
                name = name[:piece[3]]

                offset = piece[0]
                size = piece[1]
                data = self._data[offset:offset + size]
                data = _chacha20(data, key=key, nonce=nonce)

                yield bytes(name).decode(), data

    def add(self, item:Path, key:memoryview, toc_name=None) -> None:
        size = item.stat().st_size

        # find somewhere to store it or grow if we fail a lot
        offset = 0
        space = len(self._data) - size - 4096
        assert space > 0
        for i in range(100):
            offset = secrets.randbelow(space)
            offset += 4096
            for l,r in self._ranges:
                if not (offset + size <= l or offset >= r):
                    offset = 0
                    break
            else:
                self._ranges.append((offset, offset + size))
                break
        else:
            data = bytearray(self._data)
            data += bytearray(secrets.token_bytes(8 << 10))
            data = memoryview(data)
            self._set_data(data)
            return self.add(item, key, toc_name)

        key, nonce, nonce_toc = self._diffuse(key)

        # encypt the file
        with item.open("rb") as inp:
            data = _chacha20(inp.read(), key=key, nonce=nonce)

        # create a toc entry
        name = (toc_name or str(item)).encode()
        name_length = len(name)
        assert name_length <= 112
        name = name + secrets.token_bytes(112 - name_length)

        toc_entry = bytearray(128)
        toc_entry[16:] = name
        toc_entry = memoryview(toc_entry).cast("L")
        toc_entry[0] = offset
        toc_entry[1] = size
        toc_entry[2] = _hash(name)
        toc_entry[3] = name_length
        toc_entry = toc_entry.cast("B")
        toc_entry = _chacha20(toc_entry, key=key, nonce=nonce_toc)

        # find somewhere to store the toc
        while True:
            toc_index = secrets.randbelow(len(self._toc_set))
            if self._toc_set[toc_index] == 0:
                self._toc_set[toc_index] = 1
                break

        # blit encrypted parts into blob
        toc_index = toc_index * 128
        self._data[offset:offset + size] = data
        self._toc[toc_index:toc_index + 128] = toc_entry
