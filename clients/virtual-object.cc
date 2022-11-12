#include "virtual-object.h"

#include "application.h"

namespace zen::client {

bool
VirtualObject::Init()
{
  proxy_ = zgn_compositor_create_virtual_object(app_->compositor());
  if (proxy_ == nullptr) {
    zn_error("Failed to create virtual object proxy");
    return false;
  }

  return true;
}

void
VirtualObject::Commit()
{
  zgn_virtual_object_commit(proxy_);
}

VirtualObject::VirtualObject(Application *app) : app_(app) {}

VirtualObject::~VirtualObject()
{
  if (proxy_) {
    zgn_virtual_object_destroy(proxy_);
  }
}

std::unique_ptr<VirtualObject>
CreateVirtualObject(Application *app)
{
  auto virtual_object = std::make_unique<VirtualObject>(app);

  if (!virtual_object->Init()) {
    return std::unique_ptr<VirtualObject>();
  }

  return virtual_object;
}

}  // namespace zen::client