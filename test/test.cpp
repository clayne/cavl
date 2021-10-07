/// Copyright (c) 2021 Pavel Kirienko <pavel@uavcan.org>

#include "../cavl.h"
#include <unity.h>
#include <algorithm>
#include <cstdio>
#include <cstdint>
#include <optional>

void setUp() {}

void tearDown() {}

namespace
{
constexpr auto Zz       = nullptr;
constexpr auto Zzzzzzzz = Zz;

void print(const Cavl* const nd, const std::uint8_t depth = 0, const char marker = 'T')
{
    if (nd != nullptr)
    {
        for (std::uint16_t i = 0U; i < depth; i++)
        {
            std::printf("    ");
        }
        std::printf("%c=%p [%d]\n", marker, nd->value, nd->bf);
        print(nd->lr[0], depth + 1U, 'L');
        print(nd->lr[1], depth + 1U, 'R');
    }
}

template <bool Ascending, typename Node, typename Visitor>
inline void traverse(Node* const root, const Visitor& visitor)
{
    if (root != nullptr)
    {
        traverse<Ascending, Node, Visitor>(root->lr[!Ascending], visitor);
        visitor(root);
        traverse<Ascending, Node, Visitor>(root->lr[Ascending], visitor);
    }
}

std::optional<std::size_t> checkAscension(const Cavl* const root)
{
    const Cavl* prev  = nullptr;
    bool        valid = true;
    std::size_t size  = 0;
    traverse<true, const Cavl>(root, [&](const Cavl* const nd) {
        if (prev != nullptr)
        {
            valid = valid && (prev->value < nd->value);
        }
        prev = nd;
        size++;
    });
    return valid ? std::optional<std::size_t>(size) : std::optional<std::size_t>{};
}

const Cavl* findBrokenAncestry(const Cavl* const n, const Cavl* const parent = nullptr)
{
    if ((n != nullptr) && (n->up == parent))
    {
        for (auto* ch : n->lr)
        {
            if (const Cavl* p = findBrokenAncestry(ch, n))
            {
                return p;
            }
        }
        return nullptr;
    }
    return n;
}

std::uint8_t getHeight(const Cavl* const n)
{
    return (n != nullptr) ? std::uint8_t(1U + std::max(getHeight(n->lr[0]), getHeight(n->lr[1]))) : 0;
}

const Cavl* findBrokenBalanceFactor(const Cavl* const n)
{
    if (n != nullptr)
    {
        const std::int16_t hl = getHeight(n->lr[0]);
        const std::int16_t hr = getHeight(n->lr[1]);
        if (n->bf != (hr - hl))
        {
            return n;
        }
        for (auto* ch : n->lr)
        {
            if (const Cavl* p = findBrokenBalanceFactor(ch))
            {
                return p;
            }
        }
    }
    return nullptr;
}

void testCheckAscension()
{
    Cavl t{};
    Cavl l{};
    Cavl r{};
    Cavl rr{};
    t.value  = reinterpret_cast<void*>(2);
    l.value  = reinterpret_cast<void*>(1);
    r.value  = reinterpret_cast<void*>(3);
    rr.value = reinterpret_cast<void*>(4);
    // Correctly arranged tree -- smaller items on the left.
    t.lr[0] = &l;
    t.lr[1] = &r;
    r.lr[1] = &rr;
    TEST_ASSERT_EQUAL(4, checkAscension(&t));
    TEST_ASSERT_EQUAL(3, getHeight(&t));
    // Break the arrangement and make sure the breakage is detected.
    t.lr[1] = &l;
    t.lr[0] = &r;
    TEST_ASSERT_NOT_EQUAL(4, checkAscension(&t));
    TEST_ASSERT_EQUAL(3, getHeight(&t));
    TEST_ASSERT_EQUAL(&t, findBrokenBalanceFactor(&t));  // All zeros, incorrect.
    r.lr[1] = nullptr;
    TEST_ASSERT_EQUAL(2, getHeight(&t));
    TEST_ASSERT_NULL(findBrokenBalanceFactor(&t));  // Balanced now as we removed one node.
}

void testRotation()
{
    // Original state:
    //      x.left  = a
    //      x.right = z
    //      z.left  = b
    //      z.right = c
    // After left rotation of X:
    //      x.left  = a
    //      x.right = b
    //      z.left  = x
    //      z.right = c
    Cavl c{reinterpret_cast<void*>(3), Zz, {Zz, Zz}, 0};
    Cavl b{reinterpret_cast<void*>(2), Zz, {Zz, Zz}, 0};
    Cavl a{reinterpret_cast<void*>(1), Zz, {Zz, Zz}, 0};
    Cavl z{reinterpret_cast<void*>(8), Zz, {&b, &c}, 0};
    Cavl x{reinterpret_cast<void*>(9), Zz, {&a, &z}, 1};
    z.up = &x;
    c.up = &z;
    b.up = &z;
    a.up = &x;

    std::printf("Before rotation:\n");
    TEST_ASSERT_NULL(findBrokenAncestry(&x));
    TEST_ASSERT_NULL(findBrokenBalanceFactor(&x));
    print(&x);

    std::printf("After left rotation:\n");
    TEST_ASSERT_EQUAL(&z, _cavlRotate(&x, false));
    TEST_ASSERT_NULL(findBrokenAncestry(&z));
    TEST_ASSERT_NULL(findBrokenBalanceFactor(&z));
    print(&z);
    TEST_ASSERT_EQUAL(&a, x.lr[0]);
    TEST_ASSERT_EQUAL(&b, x.lr[1]);
    TEST_ASSERT_EQUAL(&x, z.lr[0]);
    TEST_ASSERT_EQUAL(&c, z.lr[1]);

    std::printf("After right rotation, back into the original configuration:\n");
    TEST_ASSERT_EQUAL(&x, _cavlRotate(&z, true));
    TEST_ASSERT_NULL(findBrokenAncestry(&x));
    TEST_ASSERT_NULL(findBrokenBalanceFactor(&x));
    print(&x);
    TEST_ASSERT_EQUAL(&a, x.lr[0]);
    TEST_ASSERT_EQUAL(&z, x.lr[1]);
    TEST_ASSERT_EQUAL(&b, z.lr[0]);
    TEST_ASSERT_EQUAL(&c, z.lr[1]);
}

void testBalancing()
{
    //     A             A           E
    //    / `           / `        /   `
    //   B   C?  =>    E   C? =>  B     A
    //  / `           / `        / `   / `
    // D?  E         B   G?     D?  F?G?  C?
    //    / `       / `
    //   F?  G?    D?  F?
    Cavl a{reinterpret_cast<void*>(1), Zz, {Zz, Zz}, -2};
    Cavl b{reinterpret_cast<void*>(2), &a, {Zz, Zz}, +1};
    Cavl c{reinterpret_cast<void*>(3), &a, {Zz, Zz}, 0};
    Cavl d{reinterpret_cast<void*>(4), &b, {Zz, Zz}, 0};
    Cavl e{reinterpret_cast<void*>(5), &b, {Zz, Zz}, 0};
    Cavl f{reinterpret_cast<void*>(6), &e, {Zz, Zz}, 0};
    Cavl g{reinterpret_cast<void*>(7), &e, {Zz, Zz}, 0};
    a.lr[0] = &b;
    a.lr[1] = &c;
    b.lr[0] = &d;
    b.lr[1] = &e;
    e.lr[0] = &f;
    e.lr[1] = &g;
    std::printf("Before balancing:");
    print(&a);
    TEST_ASSERT_NULL(findBrokenBalanceFactor(&a));
    TEST_ASSERT_NULL(findBrokenAncestry(&a));
    std::printf("After balancing:");
    TEST_ASSERT_EQUAL(&e, _cavlBalance(&a));
    print(&e);
    TEST_ASSERT_NULL(findBrokenBalanceFactor(&e));
    TEST_ASSERT_NULL(findBrokenAncestry(&e));
    TEST_ASSERT_EQUAL(&b, e.lr[0]);
    TEST_ASSERT_EQUAL(&a, e.lr[1]);
    TEST_ASSERT_EQUAL(&d, b.lr[0]);
    TEST_ASSERT_EQUAL(&f, b.lr[1]);
    TEST_ASSERT_EQUAL(&g, a.lr[0]);
    TEST_ASSERT_EQUAL(&c, a.lr[1]);
    TEST_ASSERT_EQUAL(Zz, d.lr[0]);
    TEST_ASSERT_EQUAL(Zz, d.lr[1]);
    TEST_ASSERT_EQUAL(Zz, f.lr[0]);
    TEST_ASSERT_EQUAL(Zz, f.lr[1]);
    TEST_ASSERT_EQUAL(Zz, g.lr[0]);
    TEST_ASSERT_EQUAL(Zz, g.lr[1]);
    TEST_ASSERT_EQUAL(Zz, c.lr[0]);
    TEST_ASSERT_EQUAL(Zz, c.lr[1]);
    //       A              B
    //      / `           /   `
    //     B   C?  =>    D     A
    //    / `           / `   / `
    //   D   E?        F?  G?E?  C?
    //  / `
    // F?  G?
    a = {a.value, Zz, {&b, &c}, -2};
    b = {b.value, &a, {&d, &e}, -1};
    c = {c.value, &a, {Zz, Zz}, 0};
    d = {d.value, &b, {&f, &g}, 0};
    e = {e.value, &b, {Zz, Zz}, 0};
    f = {f.value, &d, {Zz, Zz}, 0};
    g = {g.value, &d, {Zz, Zz}, 0};
    std::printf("Before balancing:");
    print(&a);
    TEST_ASSERT_NULL(findBrokenBalanceFactor(&a));
    TEST_ASSERT_NULL(findBrokenAncestry(&a));
    std::printf("After balancing:");
    TEST_ASSERT_EQUAL(&b, _cavlBalance(&a));
    print(&b);
    TEST_ASSERT_NULL(findBrokenBalanceFactor(&b));
    TEST_ASSERT_NULL(findBrokenAncestry(&b));
}

void testRetracing()
{
    Cavl t[256]{};
    //        0x50
    //       /   `
    //     0x30   0x60?
    //     /  `
    //   0x20 0x40?
    //   /
    // 0x10
    // --------------------------------------------
    //       0x30
    //      /   `
    //    0x20   0x50
    //    /      /  `
    //  0x10   0x40? 0x60?
    t[0x50] = {reinterpret_cast<void*>(0x50), Zzzzzzzz, {&t[0x30], &t[0x60]}, -1};
    t[0x30] = {reinterpret_cast<void*>(0x30), &t[0x50], {&t[0x20], &t[0x40]}, 0};
    t[0x60] = {reinterpret_cast<void*>(0x60), &t[0x50], {Zzzzzzzz, Zzzzzzzz}, 0};
    t[0x20] = {reinterpret_cast<void*>(0x20), &t[0x30], {&t[0x10], Zzzzzzzz}, 0};
    t[0x40] = {reinterpret_cast<void*>(0x40), &t[0x30], {Zzzzzzzz, Zzzzzzzz}, 0};
    t[0x10] = {reinterpret_cast<void*>(0x10), &t[0x20], {Zzzzzzzz, Zzzzzzzz}, 0};
    print(&t[0x50]);  // The tree is imbalanced because we just added 1 and are about to retrace it.
    TEST_ASSERT_NULL(findBrokenAncestry(&t[0x50]));
    TEST_ASSERT_EQUAL(6, checkAscension(&t[0x50]));
    TEST_ASSERT_EQUAL(&t[0x30], _cavlRetrace(&t[0x10], +1));
    std::puts("ADD 0x10:");
    print(&t[0x30]);  // This is the new root.
    TEST_ASSERT_EQUAL(&t[0x20], t[0x30].lr[0]);
    TEST_ASSERT_EQUAL(&t[0x50], t[0x30].lr[1]);
    TEST_ASSERT_EQUAL(&t[0x10], t[0x20].lr[0]);
    TEST_ASSERT_EQUAL(Zzzzzzzz, t[0x20].lr[1]);
    TEST_ASSERT_EQUAL(&t[0x40], t[0x50].lr[0]);
    TEST_ASSERT_EQUAL(&t[0x60], t[0x50].lr[1]);
    TEST_ASSERT_EQUAL(Zzzzzzzz, t[0x10].lr[0]);
    TEST_ASSERT_EQUAL(Zzzzzzzz, t[0x10].lr[1]);
    TEST_ASSERT_EQUAL(Zzzzzzzz, t[0x40].lr[0]);
    TEST_ASSERT_EQUAL(Zzzzzzzz, t[0x40].lr[1]);
    TEST_ASSERT_EQUAL(Zzzzzzzz, t[0x60].lr[0]);
    TEST_ASSERT_EQUAL(Zzzzzzzz, t[0x60].lr[1]);
    TEST_ASSERT_EQUAL(-1, t[0x20].bf);
    TEST_ASSERT_EQUAL(+0, t[0x30].bf);
    TEST_ASSERT_NULL(findBrokenAncestry(&t[0x30]));
    TEST_ASSERT_NULL(findBrokenBalanceFactor(&t[0x30]));
    TEST_ASSERT_EQUAL(6, checkAscension(&t[0x30]));
    // Add a new child under 0x20 and ensure that retracing stops at 0x20 because it becomes perfectly balanced:
    //
    //           0x30
    //         /      `
    //       0x20      0x50
    //       /  `      /  `
    //     0x10 0x21 0x40 0x60
    TEST_ASSERT_NULL(findBrokenAncestry(&t[0x30]));
    TEST_ASSERT_NULL(findBrokenBalanceFactor(&t[0x30]));
    t[0x21]       = {reinterpret_cast<void*>(0x21), &t[0x20], {Zzzzzzzz, Zzzzzzzz}, 0};
    t[0x20].lr[1] = &t[0x21];
    TEST_ASSERT_NULL(_cavlRetrace(&t[0x21], +1));  // Root not reached, NULL returned.
    std::puts("ADD 0x21:");
    print(&t[0x30]);
    TEST_ASSERT_EQUAL(0, t[0x20].bf);
    TEST_ASSERT_EQUAL(0, t[0x30].bf);
    TEST_ASSERT_NULL(findBrokenAncestry(&t[0x30]));
    TEST_ASSERT_NULL(findBrokenBalanceFactor(&t[0x30]));
    TEST_ASSERT_EQUAL(7, checkAscension(&t[0x30]));
    //           0x30
    //          /    `
    //       0x20     0x50
    //       / `      /  `
    //    0x10 0x21 0x40 0x60
    //       `
    //       0x15        <== first we add this, no balancing needed
    //         `
    //         0x17      <== then we add this, forcing left rotation at 0x10
    //
    // After the left rotation of 0x10, we get:
    //
    //           0x30
    //          /    `
    //       0x20     0x50
    //       / `      /  `
    //    0x15 0x21 0x40 0x60
    //     / `
    //   0x10 0x17
    //
    // When we add one extra item after 0x17, we force a double rotation (0x15 left, 0x20 right). Before the rotation:
    //
    //           0x30
    //          /    `
    //       0x20     0x50
    //       / `      /  `
    //    0x15 0x21 0x40 0x60
    //     / `
    //   0x10 0x17
    //          `
    //          0x18    <== new item causes imbalance
    //
    // After left rotation of 0x15:
    //
    //           0x30
    //         /       `
    //       0x20      0x50
    //       / `       /   `
    //     0x17 0x21 0x40 0x60
    //     / `
    //   0x15 0x18
    //   /
    // 0x10
    //
    // After right rotation of 0x20:
    //
    //           0x30
    //         /       `
    //       0x17       0x50
    //       /  `       /   `
    //    0x15  0x20  0x40 0x60
    //    /     / `
    //  0x10  0x18 0x21
    std::puts("ADD 0x15:");
    TEST_ASSERT_NULL(findBrokenAncestry(&t[0x30]));
    TEST_ASSERT_NULL(findBrokenBalanceFactor(&t[0x30]));
    TEST_ASSERT_EQUAL(7, checkAscension(&t[0x30]));
    t[0x15]       = {reinterpret_cast<void*>(0x15), &t[0x10], {Zzzzzzzz, Zzzzzzzz}, 0};
    t[0x10].lr[1] = &t[0x15];
    TEST_ASSERT_EQUAL(&t[0x30], _cavlRetrace(&t[0x15], +1));  // Same root, its balance becomes -1.
    print(&t[0x30]);
    TEST_ASSERT_EQUAL(+1, t[0x10].bf);
    TEST_ASSERT_EQUAL(-1, t[0x20].bf);
    TEST_ASSERT_EQUAL(-1, t[0x30].bf);
    TEST_ASSERT_NULL(findBrokenAncestry(&t[0x30]));
    TEST_ASSERT_NULL(findBrokenBalanceFactor(&t[0x30]));
    TEST_ASSERT_EQUAL(8, checkAscension(&t[0x30]));

    std::puts("ADD 0x17:");
    t[0x17]       = {reinterpret_cast<void*>(0x17), &t[0x15], {Zzzzzzzz, Zzzzzzzz}, 0};
    t[0x15].lr[1] = &t[0x17];
    TEST_ASSERT_EQUAL(nullptr, _cavlRetrace(&t[0x17], +1));  // Same root, same balance, 0x10 rotated left.
    print(&t[0x30]);
    // Check 0x10
    TEST_ASSERT_EQUAL(&t[0x15], t[0x10].up);
    TEST_ASSERT_EQUAL(0, t[0x10].bf);
    TEST_ASSERT_EQUAL(nullptr, t[0x10].lr[0]);
    TEST_ASSERT_EQUAL(nullptr, t[0x10].lr[1]);
    // Check 0x17
    TEST_ASSERT_EQUAL(&t[0x15], t[0x17].up);
    TEST_ASSERT_EQUAL(0, t[0x17].bf);
    TEST_ASSERT_EQUAL(nullptr, t[0x17].lr[0]);
    TEST_ASSERT_EQUAL(nullptr, t[0x17].lr[1]);
    // Check 0x15
    TEST_ASSERT_EQUAL(&t[0x20], t[0x15].up);
    TEST_ASSERT_EQUAL(0, t[0x15].bf);
    TEST_ASSERT_EQUAL(&t[0x10], t[0x15].lr[0]);
    TEST_ASSERT_EQUAL(&t[0x17], t[0x15].lr[1]);
    // Check 0x20 -- leaning left
    TEST_ASSERT_EQUAL(&t[0x30], t[0x20].up);
    TEST_ASSERT_EQUAL(-1, t[0x20].bf);
    TEST_ASSERT_EQUAL(&t[0x15], t[0x20].lr[0]);
    TEST_ASSERT_EQUAL(&t[0x21], t[0x20].lr[1]);
    // Check the root -- still leaning left by one.
    TEST_ASSERT_EQUAL(nullptr, t[0x30].up);
    TEST_ASSERT_EQUAL(-1, t[0x30].bf);
    TEST_ASSERT_EQUAL(&t[0x20], t[0x30].lr[0]);
    TEST_ASSERT_EQUAL(&t[0x50], t[0x30].lr[1]);
    //
    TEST_ASSERT_NULL(findBrokenAncestry(&t[0x30]));
    TEST_ASSERT_NULL(findBrokenBalanceFactor(&t[0x30]));
    TEST_ASSERT_EQUAL(9, checkAscension(&t[0x30]));

    std::puts("ADD 0x18:");
    t[0x18]       = {reinterpret_cast<void*>(0x18), &t[0x17], {Zzzzzzzz, Zzzzzzzz}, 0};
    t[0x17].lr[1] = &t[0x18];
    TEST_ASSERT_EQUAL(&t[0x30], _cavlRetrace(&t[0x18], +1));  // Same root, 0x15 left, 0x20 right.
    print(&t[0x30]);
}

int8_t predicate(void* const value, const Cavl* const node)
{
    if (value > node->value)
    {
        return +1;
    }
    if (value < node->value)
    {
        return -1;
    }
    return 0;
}

void testSearch()
{
    //      A
    //    B   C
    //   D E F G
    Cavl a{};
    Cavl b{};
    Cavl c{};
    Cavl d{};
    Cavl e{};
    Cavl f{};
    Cavl g{};
    a = {reinterpret_cast<void*>(4), Zz, {&b, &c}, 0};
    b = {reinterpret_cast<void*>(2), &a, {&d, &e}, 0};
    c = {reinterpret_cast<void*>(6), &a, {&f, &g}, 0};
    d = {reinterpret_cast<void*>(1), &b, {Zz, Zz}, 0};
    e = {reinterpret_cast<void*>(3), &b, {Zz, Zz}, 0};
    f = {reinterpret_cast<void*>(5), &c, {Zz, Zz}, 0};
    g = {reinterpret_cast<void*>(7), &c, {Zz, Zz}, 0};
    TEST_ASSERT_NULL(findBrokenBalanceFactor(&a));
    TEST_ASSERT_NULL(findBrokenAncestry(&a));
    TEST_ASSERT_EQUAL(7, checkAscension(&a));
    Cavl* root = &a;
    TEST_ASSERT_NULL(cavlSearch(&root, nullptr, nullptr, nullptr));         // Bad arguments.
    TEST_ASSERT_EQUAL(&a, root);                                            // Root shall not be altered.
    TEST_ASSERT_NULL(cavlSearch(&root, nullptr, predicate, nullptr));       // Item not found.
    TEST_ASSERT_EQUAL(&a, root);                                            // Root shall not be altered.
    TEST_ASSERT_EQUAL(&e, cavlSearch(&root, e.value, predicate, nullptr));  // Item found.
    TEST_ASSERT_EQUAL(&a, root);                                            // Root shall not be altered.
    print(&a);
}

}  // namespace

int main()
{
    UNITY_BEGIN();
    RUN_TEST(testCheckAscension);
    RUN_TEST(testRotation);
    RUN_TEST(testBalancing);
    RUN_TEST(testRetracing);
    RUN_TEST(testSearch);
    return UNITY_END();
}
