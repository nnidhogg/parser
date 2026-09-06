// A realistic source file for the end-to-end test: everything here is inside the study grammar and the parser's
// language, and everything the parser supports appears here at least once.

const int table_size = 256;
const int flag_mask = 10;

const char* greeting = "hello, world";
int newline = 10;

int scale_numerator = 3;

int checksum(const char* data, int length);

// Sums the bytes of a buffer, wrapping at the table size; the line comment is the grammar's only comment.
int checksum(const char* data, int length)
{
    int total = 0;

    for (int index = 0; index < length; ++index)
    {
        total = (total + data[index]) % table_size; // wrap per step
    }

    return total;
}

bool is_flagged(int value)
{
    return (value & flag_mask) != 0;
}

int classify(int value)
{
    if (is_flagged(value))
    {
        return 1;
    }
    else if (value >= table_size)
    {
        return 2;
    }

    return 0;
}

int main()
{
    int first = 0;
    int second = 0;
    int i = 0;

    while (i < 3)
    {
        first += i;
        i++;
    }

    do
    {
        i--;
        second += classify(i);
    } while (i > 0);

    int total = checksum(greeting, 12) << 1 >> 1;

    total = total > 100 ? total - 100 : total + 100;

    return static_cast<int>(total) + first * scale_numerator - second;
}
