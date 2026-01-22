///
/// test_ht.c
///
/// Testing the hashtable interface provided by adt/hashtable.h.
///

#include "../src/hashtable.c"

#include <assert.h>
#include <stdio.h>
#include <string.h>

#define NAME_LEN (32)

typedef struct person
{
	char name [NAME_LEN];
	int age;
} person;



char * names [] =
{
	"Alex",
	"Bobby",
	"Casey",
	"Donald",
	"Eric",
	"Fiona",
	"Ganondorf",
	"Hannah",
	"Indiana",
	"Jackson",
	"Kristine",
	"Loaf",
	"Mandy",
	"Nolan",
	"Oscar",
	"Penny",
	"Quentin",
	"Russ",
	"Sandy",
	"Tess",
	"Ursula",
	"Victor",
	"Wendy",
	"Xavier",
	"Your mother",
	"Zeke"
};


void print_hashtable(hashtable * ht)
{
	printf("Hashtable:\n");
	size_t len;
	person ** people = ht_values(ht, &len);
	for(size_t i=0; i<len; i++)
	{
		person * p = people[i];
		printf("Entry:\n");
		printf("\tname=%s; age=%d\n", p->name, p->age);
	}
	free(people);
}


int main()
{
	// Disable output buffering in case of crash
	setbuf(stdout, NULL);

	int retval;
	hashtable * ht;
	person p;
	person * pptr;

	printf("Testing hashtable with fixed keysize\n");
	ht = new_hashtable(NAME_LEN, HT_PAD);
	assert(ht != NULL);
	printf("ht initialized.\n");

	printf("Checking is_empty()...\n");
	assert(ht_isempty(ht));
	assert(ht_contains("anything", ht) == 0);
	printf("is_empty() is true.\n");

	printf("Inserting element...\n");
	memset(p.name, '\0', NAME_LEN);
	strcpy(p.name, "Gregory");
	p.age = 69;
	retval = ht_put(p.name, &p, ht);
	assert(retval != 1);
	printf("ht_put() returned successfully.\n");

	printf("Testing for existence of element...\n");
	assert(ht_contains(p.name, ht));
	printf("ht_contains succeeded.\n");

	printf("Retrieving element...\n");
	pptr = ht_get(p.name, ht);
	assert(pptr != NULL);
	printf("ht_get() returned successfully.\n");
	printf("Returned person {\n");
	printf("\tname=%s\n\tage=%d\n}\n", pptr->name, pptr->age);
	assert(!strcmp(pptr->name, p.name));
	assert(pptr->age == p.age);
	printf("Returned person matches initial.\n");

	printf("Inserting more elements...\n");
	// Create array of people
	person parray[16];
	printf("Initializing person");
	for(int i=0; i<16; i++)
	{
		printf(" %d;", i);
		memset(parray[i].name, '\0', NAME_LEN);
		strcpy(parray[i].name, names[i]);
		parray[i].age = i;
	}
	printf("\nPeople array initialized.\n");
	// Insert into ht
	printf("Inserting person");
	for(int i=0; i<16; i++)
	{
		printf(" %d;", i);
		retval = ht_put(parray[i].name, &parray[i], ht);
		assert(retval == 0);
	}
	printf("\nPeople successfully inserted into hashtable.\n");
	
	printf("Printing hashtable with ht_values()...\n");
	print_hashtable(ht);

	printf("Retrieving people from hashtable\n");
	for(int i=0; i<16; i++)
	{
		printf("Retrieving %s... ", names[i]);
		pptr = ht_get(names[i], ht);
		printf("Successfully retrieved %s.\n", pptr->name);
		assert(pptr != NULL);
		assert(pptr == &parray[i]);
	}

	printf("Removing %s...\n", names[1]);
	pptr = ht_remove(names[1], ht);
	printf("ht_remove() returned %s\n", pptr->name);
	assert(pptr == &parray[1]);

	printf("Attempting to fetch %s...\n", names[1]);
	pptr = ht_get(names[1], ht);
	printf("Got %p.\n", (void *)pptr);
	assert(pptr == NULL);

	printf("Removing everyone else...\n");
	for(int i=0; i<16; i++)
	{
		if(i==1)
		{
			ht_remove("Gregory", ht);
			continue;
		}
		else
			ht_remove(names[i], ht);
	}
	printf("Removed everyone.\n");
	assert(ht_isempty(ht));
	print_hashtable(ht);

	printf("Freeing hashtable...\n");
	free_hashtable(ht);
	printf("ht freed.\n");
	

	return 0;
}


