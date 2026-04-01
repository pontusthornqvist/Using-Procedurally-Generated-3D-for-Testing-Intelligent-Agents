from __future__ import annotations


class UnionFind:
    def __init__(self, n: int) -> None:
        self._p = list(range(n))

    def find(self, x: int) -> int:
        if self._p[x] != x:
            self._p[x] = self.find(self._p[x])
        return self._p[x]

    def unite(self, a: int, b: int) -> None:
        a = self.find(a)
        b = self.find(b)
        if a != b:
            self._p[a] = b
