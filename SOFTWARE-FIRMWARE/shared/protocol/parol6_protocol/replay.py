"""Bounded sequence acceptance for unordered diagnostic transports."""

from dataclasses import dataclass
from enum import Enum


class ReplayDecision(str, Enum):
    ACCEPT = "accept"
    DUPLICATE = "duplicate"
    TOO_OLD = "too_old"


@dataclass(slots=True)
class ReplayWindow:
    width: int = 64
    highest: int | None = None
    bitmap: int = 0

    def __post_init__(self) -> None:
        if not 1 <= self.width <= 256:
            raise ValueError("width must be in [1, 256]")

    def check_and_mark(self, sequence: int) -> ReplayDecision:
        if not 0 <= sequence <= 0xFFFFFFFF:
            raise ValueError("sequence must be an unsigned 32-bit value")
        if self.highest is None:
            self.highest = sequence
            self.bitmap = 1
            return ReplayDecision.ACCEPT
        if sequence > self.highest:
            shift = sequence - self.highest
            self.bitmap = 0 if shift >= self.width else self.bitmap << shift
            self.bitmap = (self.bitmap | 1) & ((1 << self.width) - 1)
            self.highest = sequence
            return ReplayDecision.ACCEPT
        offset = self.highest - sequence
        if offset >= self.width:
            return ReplayDecision.TOO_OLD
        mask = 1 << offset
        if self.bitmap & mask:
            return ReplayDecision.DUPLICATE
        self.bitmap |= mask
        return ReplayDecision.ACCEPT

