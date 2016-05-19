#pragma once

class InsuffcientResource : public std::runtime_error
{
public:
    InsuffcientResource(const char* msg)
        : std::runtime_error(msg)
    {
    }
};

class IndexManager : Uncopyable
{
public:
    static IndexManager& instance()
    {
        static IndexManager instance;
        return instance;
    }
    using IndexPair = std::pair<int, int>;
private:
    std::set<IndexPair> _indices;
    IndexManager()
        : _indices{ IndexPair(0, 0) }
    {
    }

    IndexPair allocate()
    {
        auto maxPlace = _indices.end();
        maxPlace--;
        IndexPair newPair = *maxPlace;
        if (newPair.second == std::numeric_limits<int>::max())
        {
            if (newPair.first == std::numeric_limits<int>::max())
            {
                throw InsuffcientResource("Not enough internal label to allocate.");
            }
            newPair.first++;
            newPair.second = 0;
        }
        else
        {
            newPair.second++;
        }
        _indices.insert(newPair);
        return newPair;
    }
};

