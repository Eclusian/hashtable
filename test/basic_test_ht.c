#include <stdio.h>
#include <assert.h>

#include "hashtable.h"

char * firstnames [] = {
	"Aaron",
	"Bobby",
	"Charlie",
	"Donald",
	"Eleanor",
	"Fitz",
	"Gerard",
	"Hannah",
	"Isabelle",
	"Justin",
	"Karl",
	"Lemmy",
	"Monica"
};

char * lastnames [] = {
	"Anderson",
	"Butcher",
	"Chaplin",
	"Dickinson",
	"Elaine",
	"Fitzgerald",
	"Gonzalez",
	"Haines",
	"Isembard",
	"Jackson",
	"Kim",
	"Lars",
	"Mann"
};

#define NNAMES (13)

void print_hashtable(hashtable * ht)
{
	for(size_t i=0; i<NNAMES; i++)
	{
		char * lastname = ht_get(firstnames[i], ht);
		if(lastname != NULL) printf("%s %s\n", firstnames[i], lastname);
	}
}


int main()
{
	// enum hashtable_flags flags = HT_VSIZE;
	hashtable *ht = new_hashtable(0, HT_VSIZE);
	for(int i=0; i<NNAMES; i++)
	{
		printf("Adding name \"%s\":\"%s\".\n",
				firstnames[i], lastnames[i]);
		ht_put(firstnames[i], lastnames[i], ht);
	}

	print_hashtable(ht);

	printf("Removing Hannah from the table...\n");
	ht_remove("Hannah", ht);

	print_hashtable(ht);
	
	return 0;
}
