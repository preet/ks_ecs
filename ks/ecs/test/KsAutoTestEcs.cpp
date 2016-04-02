/*
   Copyright (C) 2015 Preet Desai (preet.desai@gmail.com)

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.
*/


#include <catch/catch.hpp>

#include <random>

#include <ks/ecs/KsEcs.hpp>

// ============================================================= //

using namespace ks;

namespace ks_test_ecs {

    struct SceneKey {
        static uint const max_component_types{8};
    };

    using Scene = ecs::Scene<SceneKey>;

    template<typename ComponentType>
    using ComponentList = ecs::ComponentList<SceneKey,ComponentType>;

    // NOTE:
    // Function local types and types in anonymous namespaces
    // for ecs::Component act funny in Clang, so avoid them!
    // Possibly fixed in Clang 3.7, have to test
    struct SomeType0 {};
    struct SomeType1 {};
    struct SomeType2 {};

    struct DataABC
    {
        DataABC() :
            a(0),b(0),c(0) {}

        DataABC(int a,int b,int c) :
            a(a),b(b),c(c) {}

        int a;
        int b;
        int c;
    };

    struct DataDEF
    {
        DataDEF() :
            d(0),e(0),f(0) {}

        DataDEF(int a,int b,int c) :
            d(a),e(b),f(c) {}

        int d;
        int e;
        int f;
    };

    struct DataXYZ
    {
        DataXYZ() :
            x(0),y(0),z(0) {}

        DataXYZ(int a,int b,int c) :
            x(a),y(b),z(c) {}

        int x;
        int y;
        int z;
    };
}

using namespace ks_test_ecs;


TEST_CASE("Component indices","[ecs_cm_indices]")
{
    std::set<uint> indices;
    indices.insert(Scene::Component<SomeType0>::index);
    indices.insert(Scene::Component<SomeType0>::index);
    indices.insert(Scene::Component<SomeType1>::index);
    indices.insert(Scene::Component<SomeType1>::index);
    indices.insert(Scene::Component<SomeType2>::index);
    indices.insert(Scene::Component<SomeType2>::index);

    auto ix0 = Scene::Component<SomeType0>::index;
    REQUIRE(Scene::Component<SomeType1>::index == ix0+1);
    REQUIRE(Scene::Component<SomeType2>::index == ix0+2);

    auto mask = Scene::GetComponentMask<SomeType0,SomeType2>();
    auto check_mask = (1 << ix0) + (1 << (ix0+2));
    REQUIRE(mask == check_mask);

    bool ok = (indices.size() == 3);
    REQUIRE(ok);
}

TEST_CASE("ComponentLists","[ecs_cm_lists]")
{
    // Create scene
    shared_ptr<EventLoop> evl = make_shared<EventLoop>();
    shared_ptr<Scene> scene = MakeObject<Scene>(evl);

    // Create ComponentLists
    scene->RegisterComponentList<DataABC>(
                make_unique<ComponentList<DataABC>>(*scene));

    scene->RegisterComponentList<DataDEF>(
                make_unique<ComponentList<DataABC>>(*scene));

    scene->RegisterComponentList<DataXYZ>(
                make_unique<ComponentList<DataABC>>(*scene));

    auto ix_abc = Scene::Component<DataABC>::index;
    REQUIRE(Scene::Component<DataDEF>::index == ix_abc+1);
    REQUIRE(Scene::Component<DataXYZ>::index == ix_abc+2);

    // Get ComponentLists
    ComponentList<DataABC>* cmlist_abc =
            static_cast<ComponentList<DataABC>*>(
                scene->GetComponentList<DataABC>());

    ComponentList<DataDEF>* cmlist_def =
            static_cast<ComponentList<DataDEF>*>(
                scene->GetComponentList<DataDEF>());

    ComponentList<DataXYZ>* cmlist_xyz =
            static_cast<ComponentList<DataXYZ>*>(
                scene->GetComponentList<DataXYZ>());

    // Create some entities
    auto e0 = scene->CreateEntity();
    auto e1 = scene->CreateEntity();
    auto e2 = scene->CreateEntity();

    // Add some components
    cmlist_abc->Create(e0,DataABC{1,1,1});
    cmlist_abc->Create(e1,DataABC{2,2,2});
    cmlist_abc->Create(e2,DataABC{3,3,3});

    auto& list_sparse_abc = cmlist_abc->GetSparseList();
    REQUIRE(list_sparse_abc[e0].a == 1);
    REQUIRE(list_sparse_abc[e1].a == 2);
    REQUIRE(list_sparse_abc[e2].a == 3);

    // Remove some components
    cmlist_abc->Remove(e0);
    cmlist_abc->Remove(e2);
    REQUIRE(list_sparse_abc[e0].a == 0);
    REQUIRE(list_sparse_abc[e1].a == 2);
    REQUIRE(list_sparse_abc[e2].a == 0);
}

TEST_CASE("Component masks,","[ecs_masks]")
{
    // Create scene
    shared_ptr<EventLoop> evl = make_shared<EventLoop>();
    shared_ptr<Scene> scene = MakeObject<Scene>(evl);

    // Create ComponentLists
    scene->RegisterComponentList<DataABC>(
                make_unique<ComponentList<DataABC>>(*scene));

    scene->RegisterComponentList<DataDEF>(
                make_unique<ComponentList<DataABC>>(*scene));

    scene->RegisterComponentList<DataXYZ>(
                make_unique<ComponentList<DataABC>>(*scene));

    // Get ComponentLists
    ComponentList<DataABC>* cmlist_abc =
            static_cast<ComponentList<DataABC>*>(
                scene->GetComponentList<DataABC>());

    ComponentList<DataDEF>* cmlist_def =
            static_cast<ComponentList<DataDEF>*>(
                scene->GetComponentList<DataDEF>());

    ComponentList<DataXYZ>* cmlist_xyz =
            static_cast<ComponentList<DataXYZ>*>(
                scene->GetComponentList<DataXYZ>());

    // Create entities with some components
    for(uint i=0; i < 25; i++) {
        auto entity = scene->CreateEntity();
        cmlist_abc->Create(entity,DataABC{1,2,3});
        cmlist_def->Create(entity,DataDEF{4,5,6});
    }

    for(uint i=0; i < 50; i++) {
        auto entity = scene->CreateEntity();
        cmlist_xyz->Create(entity,DataXYZ{7,8,9});
    }

    for(uint i=0; i < 150; i++) {
        auto entity = scene->CreateEntity();
        cmlist_abc->Create(entity,DataABC{1,2,3});
        cmlist_def->Create(entity,DataDEF{4,5,6});
        cmlist_xyz->Create(entity,DataXYZ{7,8,9});
    }

    auto mask0 = Scene::GetComponentMask<DataDEF,DataABC>();
    auto mask1 = Scene::GetComponentMask<DataXYZ>();
    auto mask2 = Scene::GetComponentMask<DataDEF,DataABC,DataXYZ>();

    std::array<uint,3> list_ent_type_count;
    list_ent_type_count.fill(0);

    auto const &list_entities = scene->GetEntityList();

    for(auto const &entity : list_entities)
    {
        if(entity.valid)
        {
            if(entity.mask == mask0) {
                list_ent_type_count[0] += 1;
            }
            else if(entity.mask == mask1) {
                list_ent_type_count[1] += 1;
            }
            else if(entity.mask == mask2) {
                list_ent_type_count[2] += 1;
            }
        }
    }

    REQUIRE(list_ent_type_count[0] == 25);
    REQUIRE(list_ent_type_count[1] == 50);
    REQUIRE(list_ent_type_count[2] == 150);
}

TEST_CASE("Components","[ecs_components]")
{
    // Create scene
    shared_ptr<EventLoop> evl = make_shared<EventLoop>();
    shared_ptr<Scene> scene = MakeObject<Scene>(evl);

    // Create ComponentLists
    scene->RegisterComponentList<DataABC>(
                make_unique<ComponentList<DataABC>>(*scene));

    scene->RegisterComponentList<DataDEF>(
                make_unique<ComponentList<DataDEF>>(*scene));

    scene->RegisterComponentList<DataXYZ>(
                make_unique<ComponentList<DataXYZ>>(*scene));

    // Get ComponentLists
    ComponentList<DataABC>* cmlist_abc =
            static_cast<ComponentList<DataABC>*>(
                scene->GetComponentList<DataABC>());

    ComponentList<DataDEF>* cmlist_def =
            static_cast<ComponentList<DataDEF>*>(
                scene->GetComponentList<DataDEF>());

    ComponentList<DataXYZ>* cmlist_xyz =
            static_cast<ComponentList<DataXYZ>*>(
                scene->GetComponentList<DataXYZ>());

    // Create entities
    auto e0 = scene->CreateEntity();
    auto e1 = scene->CreateEntity();
    auto e2 = scene->CreateEntity();

    // Create components

    // e0
    cmlist_abc->Create(e0,DataABC(1,1,1));
    cmlist_def->Create(e0,DataDEF());

    // e1
    cmlist_abc->Create(e1,DataABC());
    cmlist_def->Create(e1,DataDEF());
    cmlist_xyz->Create(e1,DataXYZ());

    // e2
    cmlist_abc->Create(e2,DataABC());

    // check bitmasks
    REQUIRE((scene->GetEntityList()[e0].mask) ==
            (Scene::GetComponentMask<DataABC,DataDEF>()));

    REQUIRE((scene->GetEntityList()[e1].mask) ==
            (Scene::GetComponentMask<DataABC,DataDEF,DataXYZ>()));

    REQUIRE((scene->GetEntityList()[e2].mask) ==
            (Scene::GetComponentMask<DataABC>()));

    // Create multiple components of the same type
    // for the same entity (should overwrite)
    REQUIRE(cmlist_abc->GetComponent(e0).a == 1);

    cmlist_abc->Create(e0,DataABC(11,11,11));
    REQUIRE(cmlist_abc->GetComponent(e0).a == 11);

    cmlist_abc->Create(e0,DataABC(1,1,1));
    REQUIRE(cmlist_abc->GetComponent(e0).a == 1);

    // Remove components
    cmlist_abc->Remove(e0);
    cmlist_def->Remove(e0);
    REQUIRE(scene->GetEntityList()[e0].mask == 0);

    cmlist_def->Remove(e1);
    REQUIRE((scene->GetEntityList()[e1].mask) ==
            (Scene::GetComponentMask<DataABC,DataXYZ>()));
}
