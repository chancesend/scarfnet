#include <unity.h>
#include <Mesh.h>

void test_mesh_instantiation() {
    Scarfnet::MeshConnection connection;
    Scheduler scheduler;
    Scarfnet::Mesh mesh(connection, &scheduler);
}

void mesh_tests() {
    RUN_TEST(test_mesh_instantiation);
}