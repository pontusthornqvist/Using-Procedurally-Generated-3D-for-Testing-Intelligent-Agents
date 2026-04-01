from __future__ import annotations

from dataclasses import dataclass

# Cell values (match C++ conventions)
EMPTY = -1
CORRIDOR = -2


@dataclass(frozen=True)
class MapGenConfig:
    """Parameters for dungeon generation."""

    seed: int = 1337
    width: int = 129
    height: int = 129
    num_rooms: int = 15
    room_min_size: int = 5
    room_max_size: int = 10
    room_margin: int = 2
    slide_attempts_multiplier: int = 100

    def __post_init__(self) -> None:
        if self.width < 3 or self.height < 3:
            raise ValueError("width and height must be at least 3")
        if self.num_rooms < 1:
            raise ValueError("num_rooms must be at least 1")
        if self.room_min_size < 1:
            raise ValueError("room_min_size must be at least 1")
        if self.room_max_size < self.room_min_size:
            raise ValueError("room_max_size must be >= room_min_size")
        if self.room_margin < 0:
            raise ValueError("room_margin must be non-negative")
        if self.slide_attempts_multiplier < 0:
            raise ValueError("slide_attempts_multiplier must be non-negative")
