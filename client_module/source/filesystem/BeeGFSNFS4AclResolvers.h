#ifndef BEEGFSNFS4ACLRESOLVERS_H_
#define BEEGFSNFS4ACLRESOLVERS_H_

#include <linux/uidgid.h>

/* The state of a resolver used to indicate initialization status.
 *
 * Values:
 *  - RESOLVER_UNINITIALIZED: Initialization was never attempted. Default state after declaration
 *  - RESOLVER_READY: Initialization complete and resolver ready to use
 *  - RESOLVER_INIT_FAILED: Intermediate state to indicate errors during initialization. Can be used
 *                          for logging purposes or to retry initialization
 *  - RESOLVER_IGNORE: Resolver is not usable and shall be ignored. Resolvers go into this state if
 *                     prerequisites are not available or initialization definitively failed
 */
enum BeeGFSNFS4AclPrincipalResolverState {
   RESOLVER_UNINITIALIZED  = 0,
   RESOLVER_READY = 1,
   RESOLVER_INIT_FAILED = 2,
   RESOLVER_IGNORE = 3,
};

/* Generic principal resolver.
 *
 * Members:
 *  - .state: Indicates resolver state. See above for state definitions.
 * Methods:
 *  - .init: Initializes the resolver and sets the state accordingly
 *  - .resolve_user: Resolves a string principal to a kuid_t. Only use if state is RESOLVER_READY
 *  - .resolve_group: Resolves a string principal to a kgid_t. Only use if state is RESOLVER_READY
 */
struct BeeGFSNFS4AclPrincipalResolver {
   enum BeeGFSNFS4AclPrincipalResolverState state;
   int (*init)(struct BeeGFSNFS4AclPrincipalResolver *r);
   kuid_t (*resolve_user)(struct user_namespace *user_ns, const char *who, size_t len);
   kgid_t (*resolve_group)(struct user_namespace *user_ns, const char *who, size_t len);
};

int BeeGFSNFS4Acl_initTrivialResolver(struct BeeGFSNFS4AclPrincipalResolver *r);
kuid_t BeeGFSNFS4Acl_resolveUserTrivial(struct user_namespace *user_ns,
                                           const char *who, size_t len);
kgid_t BeeGFSNFS4Acl_resolveGroupTrivial(struct user_namespace *user_ns,
                                            const char *who, size_t len);

int BeeGFSNFS4Acl_initSidResolver(struct BeeGFSNFS4AclPrincipalResolver *r);
kuid_t BeeGFSNFS4Acl_resolveUserSid(struct user_namespace *user_ns,
                                           const char *who, size_t len);
kgid_t BeeGFSNFS4Acl_resolveGroupSid(struct user_namespace *user_ns,
                                            const char *who, size_t len);

#endif /* BEEGFSNFS4ACLRESOLVERS_H_ */
