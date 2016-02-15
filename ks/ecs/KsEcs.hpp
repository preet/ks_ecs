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

#ifndef KS_ECS_HPP
#define KS_ECS_HPP

#include <array>
#include <tuple>

#include <ks/KsObject.hpp>
#include <ks/KsLog.hpp>
#include <ks/KsException.hpp>
#include <ks/shared/KsRecycleIndexList.hpp>

namespace ks
{
    namespace ecs
    {
        namespace detail
        {
            // ============================================================= //

            constexpr uint GetMaskTypeIndex(uint bits)
            {
                return (bits <= 8) ? 0 : (bits <= 16) ? 1 : (bits <= 2) ? 2 : 3;
            }

            template<typename SceneKey>
            using Mask =
                typename std::tuple_element<
                    GetMaskTypeIndex(SceneKey::max_component_types),
                    std::tuple<u8,u16,u32,u64>
                >::type;


            // ============================================================= //

            // NOTE:
            // Function local types with ecs::Component act
            // funny in Clang, so avoid them
            template<typename SceneKey,typename T>
            struct Component
            {
                static uint const index;
            };

            // ============================================================= //

            template<typename SceneKey>
            class ComponentLimitReached : public ks::Exception
            {
            public:
                ComponentLimitReached(std::string msg) :
                    ks::Exception(ks::Exception::ErrorLevel::FATAL,std::move(msg),true)
                {}

                ~ComponentLimitReached() = default;
            };

            template<typename SceneKey>
            class ComponentCount
            {
                template<typename CmSceneKey,typename CmT>
                friend struct Component;

            private:
                template<typename T>
                static uint next()
                {
                    uint const c = increment();
                    if(c == SceneKey::max_component_types) {
                        throw ComponentLimitReached<SceneKey>(
                                    "ks::ecs::Component: Max number of "
                                    "component types reached");
                    }

                    // TODO add ifdef KS_DEBUG
                    LOG.Debug() << "ecs: Registered Component " << c
                                << ": " << typeid(T).name();
                    return c;
                }

                static uint increment()
                {
                    static uint count=0;
                    return count++;
                }
            };

            template<typename SceneKey,typename T>
            uint const Component<SceneKey,T>::index(
                    ComponentCount<SceneKey>::template next<T>());

            // ============================================================= //

            template<typename SceneKey,typename T>
            void SetComponentType(std::bitset<SceneKey::max_component_types> &mask)
            {
                mask.set(Component<SceneKey,T>::index);
            }

            template<typename SceneKey,typename... Args>
            std::bitset<SceneKey::max_component_types> GetComponentMaskBitSet()
            {
                std::bitset<SceneKey::max_component_types> mask;
                auto temp = std::initializer_list<sint>{
                    (SetComponentType<SceneKey,Args>(mask),0)... // see the comma operator
                };
                (void)temp;

                return mask;
            }

            template<typename SceneKey,typename... Args>
            Mask<SceneKey> GetComponentMask()
            {
                return static_cast<Mask<SceneKey>>(
                            GetComponentMaskBitSet<SceneKey,Args...>().to_ulong());
            }

        } // detail

        // ============================================================= //

        template<typename SceneKey>
        class Scene;

        template<typename SceneKey>
        class ComponentListBase
        {
            using Mask = detail::Mask<SceneKey>;

        public:
            ComponentListBase(Scene<SceneKey>& scene) :
                m_scene(scene)
            {}

            virtual ~ComponentListBase() = default;

            virtual void Remove(Id entity_id)=0;

        protected:
            template<typename ComponentType>
            void addComponentToEntityMask(Id entity_id);

            template<typename ComponentType>
            void removeComponentFromEntityMask(Id entity_id);

            Scene<SceneKey>& m_scene;
        };

        template<typename SceneKey>
        class Scene : public ks::Object
        {
            static_assert(SceneKey::max_component_types <= 64,
                          "ks::ecs: SceneKey: Max number of components "
                          "(64) exceeded");

        public:
            using base_type = ks::Object;

            template<typename T>
            using Component = detail::Component<SceneKey,T>;

            using Mask = detail::Mask<SceneKey>;

            struct Entity {
                bool valid{false};
                Mask mask{0};
            };

            Scene(ks::Object::Key const &key,
                  shared_ptr<EventLoop> const &evl) :
                ks::Object(key,evl)
            {
                // Reserve Entity 0 as being 'invalid'
                Id invalid_entity = CreateEntity();
                m_list_entities[invalid_entity].valid = false;
            }

            void Init(ks::Object::Key const &,
                      shared_ptr<Scene> const &)
            {

            }

            ~Scene()
            {

            }

            Id CreateEntity()
            {
                Entity entity;
                entity.valid = true;
                entity.mask = 0;

                return m_list_entities.Add(entity);
            }

            void RemoveEntity(Id id)
            {
                // Remove all components for this entity
                auto& entity = m_list_entities.Get(id);
                if(entity.mask > 0) {
                    for(uint i=0; i < SceneKey::max_component_types; i++) {
                        if(entity.mask & (Mask(1) << i)) {
                            m_list_cm_lists[i]->Remove(id);
                        }
                    }
                }

                m_list_entities.Remove(id);
            }

            std::vector<Id> GetEntityIdList() const
            {
                auto &list_entities = m_list_entities.GetList();

                std::vector<Id> m_list_ent_ids;
                m_list_ent_ids.reserve(list_entities.size());

                for(uint i=0; i < list_entities.size(); i++) {
                    if(list_entities[i].valid) {
                        m_list_ent_ids.push_back(i);
                    }
                }

                return m_list_ent_ids;
            }

            std::vector<Entity>& GetEntityList()
            {
                return m_list_entities.GetList();
            }

            std::vector<Entity> const & GetEntityList() const
            {
                return m_list_entities.GetList();
            }

            template<typename... Args>
            static Mask GetComponentMask()
            {
                return detail::GetComponentMask<SceneKey,Args...>();
            }

            template<typename ComponentType>
            void RegisterComponentList(
                    unique_ptr<ComponentListBase<SceneKey>> cm_container)
            {
                auto const idx = Component<ComponentType>::index;
                if(m_list_cm_lists[idx]==nullptr) {
                    m_list_cm_lists[idx] = std::move(cm_container);
                }
                // else { TODO }
            }

            template<typename ComponentType>
            ComponentListBase<SceneKey>* GetComponentList()
            {
                auto const idx = Component<ComponentType>::index;
                return m_list_cm_lists[idx].get();
            }

        private:
            RecycleIndexList<Entity> m_list_entities;

            std::array<
                unique_ptr<ComponentListBase<SceneKey>>,
                SceneKey::max_component_types
            > m_list_cm_lists;
        };

        // ============================================================= //

        template<typename SceneKey> template<typename ComponentType>
        void ComponentListBase<SceneKey>::addComponentToEntityMask(Id entity_id)
        {
            auto& entity = m_scene.GetEntityList()[entity_id];
            entity.mask |= (Mask(1) << detail::Component<SceneKey,ComponentType>::index);
        }

        template<typename SceneKey> template<typename ComponentType>
        void ComponentListBase<SceneKey>::removeComponentFromEntityMask(Id entity_id)
        {
            auto& entity = m_scene.GetEntityList()[entity_id];
            entity.mask &= ~(Mask(1) << detail::Component<SceneKey,ComponentType>::index);
        }

        // ============================================================= //

        template<typename SceneKey,typename ComponentType>
        class ComponentList : public ComponentListBase<SceneKey>
        {
        public:
            ComponentList(Scene<SceneKey> &scene) :
                ComponentListBase<SceneKey>(scene)
            {}

            ~ComponentList() = default;

            template<typename... Args>
            ComponentType& Create(Id entity_id, Args&&... args)
            {
                if(!(m_list_data.size() > entity_id)) {
                    m_list_data.resize(entity_id+25);
                }

                // TODO: I think this std::move is redundant,
                // right side of the equal sign is already an rval

                // TODO: Does placement new make more sense here?
                m_list_data[entity_id] = std::move(ComponentType(std::forward<Args>(args)...));

                this->template addComponentToEntityMask<ComponentType>(entity_id);

                return m_list_data[entity_id];
            }

            void Remove(Id entity_id)
            {
                // Reset component

                // TODO: I think this std::move is redundant,
                // right side of the equal sign is already an rval
                m_list_data[entity_id] = std::move(ComponentType());

                // Trim some unused component list data
                sint const diff =
                        sint(m_list_data.size())-
                        this->m_scene.GetEntityList().size();

                if(diff > 25) {
                    m_list_data.resize(m_list_data.size()-25);
                }

                this->template removeComponentFromEntityMask<ComponentType>(entity_id);
            }

            ComponentType& GetComponent(Id entity_id)
            {
                return m_list_data[entity_id];
            }

            ComponentType const & GetComponent(Id entity_id) const
            {
                return m_list_data[entity_id];
            }

            std::vector<ComponentType>& GetSparseList()
            {
                return m_list_data;
            }

            std::vector<ComponentType> const & GetSparseList() const
            {
                return m_list_data;
            }

        private:
            std::vector<ComponentType> m_list_data; // sparse list
        };

        // ============================================================= //

    }
}

#endif // KS_ECS_HPP
