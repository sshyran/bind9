/*
 * Copyright (C) 1999  Internet Software Consortium.
 * 
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS" AND INTERNET SOFTWARE CONSORTIUM DISCLAIMS
 * ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL INTERNET SOFTWARE
 * CONSORTIUM BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS
 * ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
 * SOFTWARE.
 */

/***
 *** Imports
 ***/

#include <config.h>

#include <stddef.h>
#include <string.h>

#include <isc/assertions.h>

#include <dns/db.h>
#include <dns/rdataset.h>

/***
 *** Private Types
 ***/

typedef struct {
	char *			name;	
	dns_result_t		(*create)(isc_mem_t *mctx, dns_name_t *name,
					  isc_boolean_t cache,
					  dns_rdataclass_t class,
					  unsigned int argc, char *argv[],
					  dns_db_t **dbp);
} impinfo_t;

/***
 *** Supported DB Implementations Registry
 ***/

/*
 * Supported database implementations must be registered here.
 *
 * It might be nice to generate this automatically some day.
 */

#include "rbtdb.h"

impinfo_t implementations[] = {
	{ "rbt", dns_rbtdb_create },
	{ NULL, NULL }
};

/***
 *** Basic DB Methods
 ***/

dns_result_t
dns_db_create(isc_mem_t *mctx, char *db_type, dns_name_t *origin,
	      isc_boolean_t cache, dns_rdataclass_t class,
	      unsigned int argc, char *argv[], dns_db_t **dbp)
{
	impinfo_t *impinfo;

	/*
	 * Create a new database using implementation 'db_type'.
	 */

	REQUIRE(dbp != NULL && *dbp == NULL);
	REQUIRE(dns_name_isabsolute(origin));

	for (impinfo = implementations; impinfo->name != NULL; impinfo++)
		if (strcasecmp(db_type, impinfo->name) == 0)
			return ((impinfo->create)(mctx, origin, cache, class,
						  argc, argv, dbp));

	return (DNS_R_NOTFOUND);
}

void
dns_db_attach(dns_db_t *source, dns_db_t **targetp) {

	/*
	 * Attach *targetp to source.
	 */

	REQUIRE(DNS_DB_VALID(source));
	REQUIRE(targetp != NULL);

	(source->methods->attach)(source, targetp);

	ENSURE(*targetp == source);
}

void
dns_db_detach(dns_db_t **dbp) {

	/*
	 * Detach *dbp from its database.
	 */

	REQUIRE(dbp != NULL);
	REQUIRE(DNS_DB_VALID(*dbp));

	((*dbp)->methods->detach)(dbp);

	ENSURE(*dbp == NULL);
}

isc_boolean_t
dns_db_iscache(dns_db_t *db) {

	/*
	 * Does 'db' have cache semantics?
	 */
	
	REQUIRE(DNS_DB_VALID(db));

	if ((db->attributes & DNS_DBATTR_CACHE) != 0)
		return (ISC_TRUE);

	return (ISC_FALSE);
}

isc_boolean_t
dns_db_iszone(dns_db_t *db) {

	/*
	 * Does 'db' have zone semantics?
	 */
	
	REQUIRE(DNS_DB_VALID(db));

	if ((db->attributes & DNS_DBATTR_CACHE) == 0)
		return (ISC_TRUE);

	return (ISC_FALSE);
}

dns_name_t *
dns_db_origin(dns_db_t *db) {
	/*
	 * The origin of the database.
	 */

	REQUIRE(DNS_DB_VALID(db));

	return (&db->origin);
}

dns_result_t
dns_db_load(dns_db_t *db, char *filename) {
	/*
	 * Load master file 'filename' into 'db'.
	 */

	REQUIRE(DNS_DB_VALID(db));
	REQUIRE((db->attributes & DNS_DBATTR_LOADED) == 0);

	return (db->methods->load(db, filename));
}

/***
 *** Version Methods
 ***/

void
dns_db_currentversion(dns_db_t *db, dns_dbversion_t **versionp) {
	
	/*
	 * Open the current version for reading.
	 */
	
	REQUIRE(DNS_DB_VALID(db));
	REQUIRE(versionp != NULL && *versionp == NULL);

	(db->methods->currentversion)(db, versionp);
}

dns_result_t
dns_db_newversion(dns_db_t *db, dns_dbversion_t **versionp) {
	
	/*
	 * Open a new version for reading and writing.
	 */

	REQUIRE(DNS_DB_VALID(db));
	REQUIRE(versionp != NULL && *versionp == NULL);

	return ((db->methods->newversion)(db, versionp));
}

void
dns_db_closeversion(dns_db_t *db, dns_dbversion_t **versionp,
		    isc_boolean_t commit)
{
	
	/*
	 * Close version '*versionp'.
	 */

	REQUIRE(DNS_DB_VALID(db));
	REQUIRE(versionp != NULL && *versionp != NULL);

	(db->methods->closeversion)(db, versionp, commit);

	ENSURE(*versionp == NULL);
}

/***
 *** Node Methods
 ***/

dns_result_t
dns_db_findnode(dns_db_t *db, dns_name_t *name,
		isc_boolean_t create, dns_dbnode_t **nodep)
{

	/*
	 * Find the node with name 'name'.
	 *
	 * WARNING:  THIS API WILL BE CHANGING IN THE NEAR FUTURE.
	 *
	 * XXX Add options parameter (e.g. so we can say "longest match").
	 */

	REQUIRE(DNS_DB_VALID(db));
	REQUIRE(dns_name_issubdomain(name, &db->origin));
	REQUIRE(nodep != NULL && *nodep == NULL);

	return ((db->methods->findnode)(db, name, create, nodep));
}

void
dns_db_attachnode(dns_db_t *db, dns_dbnode_t *source, dns_dbnode_t **targetp) {

	/*
	 * Attach *targetp to source.
	 */

	REQUIRE(DNS_DB_VALID(db));
	REQUIRE(source != NULL);
	REQUIRE(targetp != NULL && *targetp == NULL);

	(db->methods->attachnode)(db, source, targetp);
}

void
dns_db_detachnode(dns_db_t *db, dns_dbnode_t **nodep) {

	/*
	 * Detach *nodep from its node.
	 */

	REQUIRE(DNS_DB_VALID(db));
	REQUIRE(nodep != NULL && *nodep != NULL);

	(db->methods->detachnode)(db, nodep);

	ENSURE(*nodep == NULL);
}

/***
 *** Rdataset Methods
 ***/

dns_result_t
dns_db_findrdataset(dns_db_t *db, dns_dbnode_t *node, dns_dbversion_t *version,
		    dns_rdatatype_t type, dns_rdataset_t *rdataset)
{
	/*
	 * Search for an rdataset of type 'type' at 'node' that are in version
	 * 'version' of 'db'.  If found, make 'rdataset' refer to it.
	 */

	REQUIRE(DNS_DB_VALID(db));
	REQUIRE(node != NULL);
	REQUIRE(DNS_RDATASET_VALID(rdataset));
	REQUIRE(rdataset->methods == NULL);

	return ((db->methods->findrdataset)(db, node, version, type,
					    rdataset));
}

dns_result_t
dns_db_addrdataset(dns_db_t *db, dns_dbnode_t *node, dns_dbversion_t *version,
		   dns_rdataset_t *rdataset)
{
	/*
	 * Add 'rdataset' to 'node' in version 'version' of 'db'.
	 */

	REQUIRE(DNS_DB_VALID(db));
	REQUIRE(node != NULL);
	REQUIRE(version != NULL);
	REQUIRE(DNS_RDATASET_VALID(rdataset));
	REQUIRE(rdataset->methods != NULL);

	return ((db->methods->addrdataset)(db, node, version, rdataset));
}

dns_result_t
dns_db_deleterdataset(dns_db_t *db, dns_dbnode_t *node,
		      dns_dbversion_t *version, dns_rdatatype_t type)
{
	/*
	 * Make it so that no rdataset of type 'type' exists at 'node' in
	 * version version 'version' of 'db'.
	 */

	REQUIRE(DNS_DB_VALID(db));
	REQUIRE(node != NULL);
	REQUIRE(version != NULL);

	return ((db->methods->deleterdataset)(db, node, version, type));
}
