int main() {

  cache::MetadataStore store;
  cache::NodeRegistry registry;

  registry.RegisterNode({"node-a", "127.0.0.1:9001"});
  registry.RegisterNode({"node-b", "127.0.0.1:9002"});

  cache::Router router(store, registry);

  auto node = router.RouteRequest("sess-1", "llama", "prefix1");

  if (node)
    std::cout << "route to: " << *node << "\n";

}