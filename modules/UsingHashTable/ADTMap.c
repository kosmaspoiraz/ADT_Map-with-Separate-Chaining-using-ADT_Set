/////////////////////////////////////////////////////////////////////////////
//
// Υλοποίηση του ADT Map μέσω Hash Table με open addressing (linear probing)
//
/////////////////////////////////////////////////////////////////////////////

#include <stdlib.h>
#include <stdio.h>

#include "ADTMap.h"
#include "ADTSet.h"

// Το μέγεθος του Hash Table ιδανικά θέλουμε να είναι πρώτος αριθμός σύμφωνα με την θεωρία.
// Η παρακάτω λίστα περιέχει πρώτους οι οποίοι έχουν αποδεδιγμένα καλή συμπεριφορά ως μεγέθη.
// Κάθε re-hash θα γίνεται βάσει αυτής της λίστας. Αν χρειάζονται παραπάνω απο 1610612741 στοχεία, τότε σε καθε rehash διπλασιάζουμε το μέγεθος.
int prime_sizes[] = {53, 97, 193, 389, 769, 1543, 3079, 6151, 12289, 24593, 49157, 98317, 196613, 393241,
					 786433, 1572869, 3145739, 6291469, 12582917, 25165843, 50331653, 100663319, 201326611, 402653189, 805306457, 1610612741};

// Χρησιμοποιούμε open addressing, οπότε σύμφωνα με την θεωρία, πρέπει πάντα να διατηρούμε
// τον load factor του  hash table μικρότερο ή ίσο του 0.5, για να έχουμε αποδoτικές πράξεις
#define MAX_LOAD_FACTOR 0.5

// Δομή του κάθε κόμβου που έχει το hash table (με το οποίο υλοιποιούμε το map)
struct map_node
{
	Pointer key;   // Το κλειδί που χρησιμοποιείται για να hash-αρουμε
	Pointer value; // Η τιμή που αντισοιχίζεται στο παραπάνω κλειδί
};

// Δομή του Map (περιέχει όλες τις πληροφορίες που χρεαζόμαστε για το HashTable)
struct map
{
	Set *array;				 // Ο πίνακας που θα χρησιμοποιήσουμε για το map (remember, φτιάχνουμε ένα hash table)
	int capacity;			 // Πόσο χώρο έχουμε δεσμεύσει.
	int size;				 // Πόσα στοιχεία έχουμε προσθέσει
	CompareFunc compare;	 // Συνάρτηση για σύγκρηση δεικτών, που πρέπει να δίνεται απο τον χρήστη
	HashFunc hash_function;	 // Συνάρτηση για να παίρνουμε το hash code του κάθε αντικειμένου.
	DestroyFunc destroy_key; // Συναρτήσεις που καλούνται όταν διαγράφουμε έναν κόμβο απο το map.
	DestroyFunc destroy_value;
};

void set_destroy_map_node(DestroyFunc destroy_key, DestroyFunc destroy_value, MapNode node)
{
	if (destroy_key != NULL)
		destroy_key(node->key);
	if (destroy_value != NULL)
		destroy_value(node->value);
}

Map map_create(CompareFunc compare, DestroyFunc destroy_key, DestroyFunc destroy_value)
{
	// Δεσμεύουμε κατάλληλα τον χώρο που χρειαζόμαστε για το hash table
	Map map = malloc(sizeof(*map));
	map->capacity = prime_sizes[0];
	map->size = 0;
	map->compare = compare;
	map->destroy_key = destroy_key;
	map->destroy_value = destroy_value;

	map->array = malloc(map->capacity * sizeof(struct map_node));
	for (int i = 0; i < map->capacity; i++)
		map->array[i] = set_create(map->compare, map->destroy_value);

	return map;
}

// Επιστρέφει τον αριθμό των entries του map σε μία χρονική στιγμή.
int map_size(Map map)
{
	return map->size;
}

// Συνάρτηση για την επέκταση του Hash Table σε περίπτωση που ο load factor μεγαλώσει πολύ.
static void rehash(Map map)
{
	// Αποθήκευση των παλιών δεδομένων
	int old_capacity = map->capacity;
	Set *old_array = map->array;

	// Βρίσκουμε τη νέα χωρητικότητα, διασχίζοντας τη λίστα των πρώτων ώστε να βρούμε τον επόμενο.
	int prime_no = sizeof(prime_sizes) / sizeof(int); // το μέγεθος του πίνακα
	for (int i = 0; i < prime_no; i++)
	{ // LCOV_EXCL_LINE
		if (prime_sizes[i] > old_capacity)
		{
			map->capacity = prime_sizes[i];
			break;
		}
	}
	// Αν έχουμε εξαντλήσει όλους τους πρώτους, διπλασιάζουμε
	if (map->capacity == old_capacity) // LCOV_EXCL_LINE
		map->capacity *= 2;			   // LCOV_EXCL_LINE

	// Δημιουργούμε ένα μεγαλύτερο hash table
	map->array = malloc(map->capacity * sizeof(struct map_node));
	for (int i = 0; i < map->capacity; i++)
		map->array[i] = set_create(map->compare, map->destroy_key);

	// Τοποθετούμε ΜΟΝΟ τα entries που όντως περιέχουν ένα στοιχείο (το rehash είναι και μία ευκαιρία να ξεφορτωθούμε τα deleted nodes)
	map->size = 0;
	for (int i = 0; i < old_capacity; i++)
		for (SetNode node = set_first(old_array[i]); node != SET_EOF; node = set_next(old_array[i], node))
		{
			MapNode mnode = set_node_value(old_array[i], node);
			set_insert(map->array[i], mnode);
			map->size++;
		}

	//Αποδεσμεύουμε τον παλιό πίνακα ώστε να μήν έχουμε leaks
	free(old_array);
}

// Εισαγωγή στο hash table του ζευγαριού (key, item). Αν το key υπάρχει,
// ανανέωση του με ένα νέο value, και η συνάρτηση επιστρέφει true.

void map_insert(Map map, Pointer key, Pointer value)
{
	// Αν με την νέα εισαγωγή ξεπερνάμε το μέγιστο load factor, πρέπει να κάνουμε rehash
	float load_factor = (float)map->size / map->capacity;
	if (load_factor > MAX_LOAD_FACTOR)
		rehash(map);

	uint pos = map->hash_function(key) % map->capacity;

	MapNode node = malloc(sizeof(MapNode));
	node->key = key;
	node->value = value;
	if (map->array[pos] == NULL)
	{
		map->array[pos] = set_create(map->compare, map->destroy_value);
		set_insert(map->array[pos], node);
		map->size++;
	}
	else if (set_find(map->array[pos], value) != NULL)
	{
		set_destroy_map_node(map->destroy_key, map->destroy_value, set_find(map->array[pos], value));
		set_insert(map->array[pos], node);
		map->size++;
	}
	else
	{
		set_insert(map->array[pos], node);
		map->size++;
	}
}

// Διαργραφή απο το Hash Table του κλειδιού με τιμή key
bool map_remove(Map map, Pointer key)
{
	uint pos = map->hash_function(key) % map->capacity;
	for (SetNode node = set_first(map->array[pos]); node != SET_EOF; node = set_next(map->array[pos], node))
	{
		MapNode mnode = set_node_value(map->array[pos], node);
		if (*(uint *)mnode->key != pos)
			return false;
		set_destroy_map_node(map->destroy_key, map->destroy_value, mnode);

		map->size--;
		return true;
	}

	return false;
}

// Αναζήτηση στο map, με σκοπό να επιστραφεί το value του κλειδιού που περνάμε σαν όρισμα.

Pointer map_find(Map map, Pointer key)
{
	int count = 0;
	for (uint pos = map->hash_function(key) % map->capacity; count <= map->size; pos = (pos + 1) % map->capacity)
	{
		for (SetNode node = set_first(map->array[pos]); node != SET_EOF; node = set_next(map->array[pos], node))
		{
			MapNode mnode = set_node_value(map->array[pos], node);
			if (mnode->key == key)
				return mnode->value;
		}
		count++;
	}
	return NULL;
}

DestroyFunc map_set_destroy_key(Map map, DestroyFunc destroy_key)
{
	DestroyFunc old = map->destroy_key;
	map->destroy_key = destroy_key;
	return old;
}

DestroyFunc map_set_destroy_value(Map map, DestroyFunc destroy_value)
{
	DestroyFunc old = map->destroy_value;
	map->destroy_value = destroy_value;
	return old;
}

// Απελευθέρωση μνήμης που δεσμεύει το map
void map_destroy(Map map)
{
	for (int i = 0; i < map->capacity; i++)
		set_destroy(map->array[i]);

	free(map->array);
	free(map);
}

/////////////////////// Διάσχιση του map μέσω κόμβων ///////////////////////////

MapNode map_first(Map map)
{
	//Ξεκινάμε την επανάληψή μας απο το 1ο στοιχείο, μέχρι να βρούμε κάτι όντως τοποθετημένο
	for (int i = 0; i < map->capacity; i++)
		if (map->array[i] == NULL)
			continue;
		else if (map->array[i] == NULL)
			return set_node_value(map->array[i], set_first(map->array[i]));

	return MAP_EOF;
}

MapNode map_next(Map map, MapNode node)
{
	for (int i = 0; i <= map->size; i++)
	{
		SetNode snode = set_find_node(map->array[i], node->value);
		if (snode == NULL)
			continue;
		if (set_next(map->array[i], snode) != SET_EOF)
		{
			MapNode mnode = set_node_value(map->array[i], set_next(map->array[i], snode));
			return mnode;
		}
		else
		{
			for (int j = i + 1; j <= map->size; j++)
				if (map->array[j] != NULL || set_first(map->array[j]) != SET_EOF)
				{
					MapNode mnode = set_node_value(map->array[j], set_first(map->array[j]));
					return mnode;
				}
		}
	}

	return MAP_EOF;
}

Pointer map_node_key(Map map, MapNode node)
{
	return node->key;
}

Pointer map_node_value(Map map, MapNode node)
{
	return node->value;
}

MapNode map_find_node(Map map, Pointer key)
{
	int count = 0;
	for (uint pos = map->hash_function(key) % map->capacity; count <= map->size; pos = (pos + 1) % map->capacity)
	{
		for (SetNode node = set_first(map->array[pos]); node != SET_EOF; node = set_next(map->array[pos], node))
		{
			MapNode mnode = set_node_value(map->array[pos], node);
			if (mnode->key == key)
				return mnode;
		}
		count++;
	}

	return MAP_EOF;
}

// Αρχικοποίηση της συνάρτησης κατακερματισμού του συγκεκριμένου map.
void map_set_hash_function(Map map, HashFunc func)
{
	map->hash_function = func;
}

uint hash_string(Pointer value)
{
	// djb2 hash function, απλή, γρήγορη, και σε γενικές γραμμές αποδοτική
	uint hash = 5381;
	for (char *s = value; *s != '\0'; s++)
		hash = (hash << 5) + hash + *s; // hash = (hash * 33) + *s. Το foo << 5 είναι γρηγορότερη εκδοχή του foo * 32.
	return hash;
}

uint hash_int(Pointer value)
{
	return *(int *)value;
}

uint hash_pointer(Pointer value)
{
	return (size_t)value; // cast σε sizt_t, που έχει το ίδιο μήκος με έναν pointer
}