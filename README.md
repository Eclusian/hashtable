# Attention!

**This is not my main upstream repository for this project and so may not be completely up to date.**

This is an implementation of a hashtable in C. It stores references to user data and retrieves them based on keys, which can be either null-terminated strings or fixed-size data blobs.

Here is an example usage of the library:

```C
#include "hashtable.h"
#include <stdio.h>

struct food_t {
  char *name;
  char *flavor;
};

struct food_t apple = {"Apple", "Sweet"};
struct food_t lemon = {"Lemon", "Sour"};
struct food_t pizza = {"Pizza", "Savory"};

int main(void)
{
  struct food_t *f;
  hashtable *ht = new_hashtable(0, HT_VSIZE);
  ht_put(apple.name, &apple, ht);
  ht_put(lemon.name, &lemon, ht);
  ht_put(pizza.name, &pizza, ht);

  f = ht_get("Apple", ht);
  printf("%s is a %s food.\n", f->name, f->flavor);

  if(!ht_contains("Orange", ht)) {
    printf("Error: Cannot compare apples and oranges.\n");
  }

  free_hashtable(ht);
  return 0;
}
```
