namespace MMAI::BAI::V15::Graph
{
    inline int permille(int a, int b)
    {
        // Multiplying by 1000 might cause int32 overflow
        // => use temp l
        return static_cast<int>((1000LL * a) / b);
    }
}
