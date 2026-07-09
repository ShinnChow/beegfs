#ifndef FHGFSXATTRHANDLERS_H_
#define FHGFSXATTRHANDLERS_H_

#include <linux/xattr.h>

int FhgfsXAttr_init_security(struct inode *inode, struct inode *dir, const struct qstr *qstr);

extern const struct xattr_handler fhgfs_xattr_user_handler;
#ifdef KERNEL_HAS_GET_ACL
extern const struct xattr_handler fhgfs_xattr_acl_access_handler;
extern const struct xattr_handler fhgfs_xattr_acl_default_handler;
#endif
extern const struct xattr_handler beegfsXAttrNfs4AclHandler;
extern const struct xattr_handler fhgfs_xattr_security_handler;

/**
 * The get-function which is used for all the security.* xattrs.
 */
#if defined(KERNEL_HAS_XATTR_HANDLERS_INODE_ARG)
int FhgfsXAttr_getSecurity(const struct xattr_handler* handler, struct dentry* dentry,
      struct inode* inode, const char* name, void* value, size_t size);
#elif defined(KERNEL_HAS_XATTR_HANDLER_PTR_ARG)
int FhgfsXAttr_getSecurity(const struct xattr_handler* handler, struct dentry* dentry,
      const char* name, void* value, size_t size);
#endif

#endif /* FHGFSXATTRHANDLERS_H_ */
