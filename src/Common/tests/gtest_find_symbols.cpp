#include <string>
#include <vector>
#include <base/find_symbols.h>
#include <gtest/gtest.h>


template <char ... symbols>
void test_find_first_not(std::string_view haystack, std::string_view::iterator expected_output)
{
    ASSERT_EQ(expected_output, find_first_not_symbols<symbols...>(haystack.begin(), haystack.end()));
}

TEST(FindSymbols, SimpleTest)
{
    std::string s = "Hello, world! Goodbye...";
    const char * begin = s.data();
    const char * end = s.data() + s.size();

    ASSERT_EQ(find_first_symbols<'a'>(begin, end), end);
    ASSERT_EQ(find_first_symbols<'e'>(begin, end), begin + 1);
    ASSERT_EQ(find_first_symbols<'.'>(begin, end), begin + 21);
    ASSERT_EQ(find_first_symbols<' '>(begin, end), begin + 6);
    ASSERT_EQ(find_first_symbols<'H'>(begin, end), begin);
    ASSERT_EQ((find_first_symbols<'a', 'e'>(begin, end)), begin + 1);

    ASSERT_EQ(find_last_symbols_or_null<'a'>(begin, end), nullptr);
    ASSERT_EQ(find_last_symbols_or_null<'e'>(begin, end), end - 4);
    ASSERT_EQ(find_last_symbols_or_null<'.'>(begin, end), end - 1);
    ASSERT_EQ(find_last_symbols_or_null<' '>(begin, end), end - 11);
    ASSERT_EQ(find_last_symbols_or_null<'H'>(begin, end), begin);
    ASSERT_EQ((find_last_symbols_or_null<'a', 'e'>(begin, end)), end - 4);

    {
        std::vector<std::string> vals;
        splitInto<' ', ','>(vals, "hello, world", true);
        ASSERT_EQ(vals, (std::vector<std::string>{"hello", "world"}));
    }

    {
        std::vector<std::string> vals;
        splitInto<' ', ','>(vals, "s String", true);
        ASSERT_EQ(vals, (std::vector<std::string>{"s", "String"}));
    }
}

TEST(FindNotSymbols, AllSymbolsPresent)
{
    std::string_view str_with_17_bytes = "hello world hello";
    std::string_view str_with_16_bytes = {str_with_17_bytes.begin(), str_with_17_bytes.end() - 1u};
    std::string_view str_with_15_bytes = {str_with_16_bytes.begin(), str_with_16_bytes.end() - 1u};

    /*
     * The below variations will choose different implementation strategies:
     * 1. Loop method only because it does not contain enough bytes for SSE 4.2
     * 2. SSE4.2 only since string contains exactly 16 bytes
     * 3. SSE4.2 + Loop method will take place because only first 16 bytes are treated by SSE 4.2 and remaining bytes is treated by loop
     *
     * Below code asserts that all calls return the ::end of the input string. This was not true prior to this fix as mentioned in PR #47304
     * */

    test_find_first_not<'h', 'e', 'l', 'o', 'w', 'r', 'd', ' '>(str_with_15_bytes, str_with_15_bytes.end());
    test_find_first_not<'h', 'e', 'l', 'o', 'w', 'r', 'd', ' '>(str_with_16_bytes, str_with_16_bytes.end());
    test_find_first_not<'h', 'e', 'l', 'o', 'w', 'r', 'd', ' '>(str_with_17_bytes, str_with_17_bytes.end());
}

TEST(FindNotSymbols, NoSymbolsMatch)
{
    std::string_view s = "abcdefg";

    // begin should be returned since the first character of the string does not match any of the below symbols
    test_find_first_not<'h', 'i', 'j'>(s, s.begin());
}

TEST(FindNotSymbols, ExtraSymbols)
{
    std::string_view s = "hello_world_hello";
    test_find_first_not<'h', 'e', 'l', 'o', ' '>(s, s.begin() + 5u);
}

TEST(FindNotSymbols, EmptyString)
{
    std::string_view s = "";
    test_find_first_not<'h', 'e', 'l', 'o', 'w', 'r', 'd', ' '>(s, s.end());
}

TEST(FindNotSymbols, SingleChar)
{
    std::string_view s = "a";
    test_find_first_not<'a'>(s, s.end());
}

TEST(FindNotSymbols, NullCharacter)
{
    // special test to ensure only the passed template arguments are used as needles
    // since current find_first_symbols implementation takes in 16 characters and defaults
    // to \0.
    auto s = std::string_view("abcdefg\0x", 9u);
    test_find_first_not<'a', 'b', 'c', 'd', 'e', 'f', 'g'>(s, s.begin() + 7u);
}
