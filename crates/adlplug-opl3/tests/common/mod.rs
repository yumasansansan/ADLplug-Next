// SPDX-FileCopyrightText: 2026 Yuma Kakei <yumasansansan@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later
//
// This file is part of ADLplug-Next. The SPDX lines name its copyright holder
// and its license, in the machine-readable form of the REUSE specification: the
// GNU General Public License, version 3 or any later version
// (LICENSES/GPL-3.0-or-later.txt).

//! What the core's tests feed it: random writes at random times, as the C++
//! version's tests made them (`tests/unit/opl3_core_tests.cc` of the C++
//! version, `random_writes`), the same writes for the same seed, and the soft
//! panning of a channel every 500 samples that its test of the hand-written
//! passes added. The C++ core's samples for these are what the Rust core's are
//! to equal (`cpp_core.rs`).

/// A register write, before the sample it is at.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct Write {
    /// The sample the write comes before.
    pub at: u32,
    /// The register's address, the second bank's from 0x100.
    pub address: u16,
    /// What is written.
    pub value: u8,
}

/// The xorshift generator of the C++ tests, and the numbers below a bound
/// that they take from it.
struct Generator {
    state: u64,
}

impl Generator {
    fn new(seed: u32) -> Self {
        Self {
            state: 0x9e37_79b9_7f4a_7c15_u64.wrapping_mul(u64::from(seed) + 1),
        }
    }

    /// The next number: the low 32 bits of the state after three shifts.
    fn next(&mut self) -> u32 {
        self.state ^= self.state << 13;
        self.state ^= self.state >> 7;
        self.state ^= self.state << 17;
        u32::try_from(self.state & u64::from(u32::MAX)).expect("the low 32 bits")
    }

    fn below(&mut self, n: u32) -> u32 {
        self.next() % n
    }

    /// The low byte of the next number.
    fn byte(&mut self) -> u8 {
        low_byte(self.next())
    }
}

fn low_byte(value: u32) -> u8 {
    u8::try_from(value & 0xff).expect("a byte")
}

fn address(value: u32) -> u16 {
    u16::try_from(value).expect("an address of the chip")
}

/// The offsets of the 18 operators of a bank from their registers' base.
const OP_OFFSET: [u32; 18] = [0, 1, 2, 3, 4, 5, 8, 9, 10, 11, 12, 13, 16, 17, 18, 19, 20, 21];

/// The offset of operator `op` of the chip's 36 from its registers' base, the
/// second bank's from 0x100.
fn operator_offset(op: u32) -> u32 {
    let bank = if op >= 18 { 0x100 } else { 0 };
    bank + OP_OFFSET[usize::try_from(op % 18).expect("an operator")]
}

/// Random writes at random times, somewhat as a player makes them: notes on
/// and off with random voices, pitch, level and panning changes, drums, the
/// registers of the whole chip now and then, and any address at all but the
/// test registers, which the core does not have. The numbers are drawn in the
/// order the C++ version drew them, the left operand before the right where an
/// expression draws two.
#[must_use]
pub fn random_writes(seed: u32, samples: u32) -> Vec<Write> {
    let mut g = Generator::new(seed);
    let mut w = Vec::new();
    let mut add = |at: u32, address: u32, value: u8| {
        w.push(Write {
            at,
            address: self::address(address),
            value,
        });
    };
    add(0, 0x105, 1);
    add(0, 0x01, 0x20);
    let mut fnum = [0_u32; 18];
    let mut block = [0_u32; 18];
    let mut at = 0;
    while at < samples {
        let kind = g.below(100);
        if kind < 30 {
            let ch = g.below(18);
            let (c, bank) = (ch % 9, ch / 9);
            let first = bank * 18 + (c / 3) * 6 + c % 3;
            for op in [first, first + 3] {
                let off = operator_offset(op);
                let value = g.byte();
                add(at, 0x20 + off, value);
                let value = if g.below(3) == 0 {
                    g.byte()
                } else {
                    low_byte(g.below(40))
                };
                add(at, 0x40 + off, value);
                let value = g.byte();
                add(at, 0x60 + off, value);
                let value = g.byte();
                add(at, 0x80 + off, value);
                let value = low_byte(g.below(8));
                add(at, 0xe0 + off, value);
            }
            let base = bank * 0x100 + c;
            let connection = g.byte();
            let value = connection | if g.below(4) == 0 { 0 } else { 0x30 };
            add(at, 0xc0 + base, value);
            let i = usize::try_from(ch).expect("a channel");
            fnum[i] = g.below(1024);
            block[i] = g.below(8);
            add(at, 0xa0 + base, low_byte(fnum[i]));
            add(at, 0xb0 + base, low_byte(0x20 | (block[i] << 2) | (fnum[i] >> 8)));
        } else if kind < 50 {
            let ch = g.below(18);
            let i = usize::try_from(ch).expect("a channel");
            add(
                at,
                0xb0 + (ch / 9) * 0x100 + ch % 9,
                low_byte((block[i] << 2) | (fnum[i] >> 8)),
            );
        } else if kind < 60 {
            let ch = g.below(18);
            let base = (ch / 9) * 0x100 + ch % 9;
            let i = usize::try_from(ch).expect("a channel");
            fnum[i] = (fnum[i] + g.below(64) + 1024 - 32) & 1023;
            add(at, 0xa0 + base, low_byte(fnum[i]));
            add(at, 0xb0 + base, low_byte(0x20 | (block[i] << 2) | (fnum[i] >> 8)));
        } else if kind < 70 {
            let op = g.below(36);
            let value = g.byte();
            add(at, 0x40 + operator_offset(op), value);
        } else if kind < 78 {
            let ch = g.below(18);
            let value = g.byte();
            add(at, 0xc0 + (ch / 9) * 0x100 + ch % 9, value);
        } else if kind < 88 {
            let value = g.byte();
            add(at, 0xbd, value);
        } else if kind < 91 {
            let value = low_byte(g.below(64));
            add(at, 0x104, value);
        } else if kind < 92 {
            let value = u8::from(g.below(4) != 0);
            add(at, 0x105, value);
        } else if kind < 94 {
            const REGS: [u32; 7] = [0x08, 0x02, 0x03, 0x04, 0x102, 0x103, 0x108];
            let address = REGS[usize::try_from(g.below(7)).expect("a register")];
            let value = g.byte();
            add(at, address, value);
        } else {
            let mut address = g.below(0x200);
            if (address & 0xff) == 0x01 {
                address ^= 0x02;
            }
            let value = g.byte();
            add(at, address, value);
        }
        at += if g.below(4) == 0 {
            0
        } else if g.below(3) == 0 {
            g.below(400)
        } else {
            g.below(40)
        };
    }
    w
}

/// The soft panning of the C++ version's test of the hand-written passes, for
/// sample `n` of seed `seed`: every 500 samples a channel's, and every third
/// time back to the centre. The address is the channel's 0xc0 register's, less
/// 0xc0.
#[must_use]
pub fn soft_pan(n: u32, seed: u32) -> Option<(u16, u8)> {
    if !n.is_multiple_of(500) {
        return None;
    }
    let ch = (n / 500 * 7 + seed) % 18;
    let pan = if n.is_multiple_of(1500) {
        64
    } else {
        (n / 500 * 37 + seed * 11) % 128
    };
    Some((address((ch / 9) * 0x100 + ch % 9), low_byte(pan)))
}
