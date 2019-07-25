#pragma once

//#include <stdint.h>

// typedef unsigned long long UID;
// This has exactly the same footprint as the typedef above and behaves exactly the same with the exception,
// that on the UID class we can have some more spart behaviour as on a simple typedef.
class UID
{
private:
  unsigned long long uid;
public:
  UID(const unsigned long long u = 0)
    : uid(u)
  {
  }

  UID(const unsigned short manuf, const unsigned int device)
    : uid((manuf<<32) | device)
  {
  }

  operator unsigned long long () const
  {
    return uid;
  }

  UID & operator = (const unsigned long long u)
  {
    uid = u;
    return *this;
  }

  bool operator == (const UID & u) const
  {
    return uid == u.uid;
  }

  bool operator > (const UID & u) const
  {
    return uid > u.uid;
  }
};
