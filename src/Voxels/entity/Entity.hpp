#pragma once

#include <memory>

class Entity;

class Component {
public:
    friend class Entity;

    [[nodiscard]] Entity* getEntity() const {
        return entity;
    }

    Component() = default;
    Component(const Component&) = delete;
    Component(Component&&) = delete;
    Component& operator=(const Component&) = delete;
    Component& operator=(Component&&) = delete;
    virtual ~Component() = default;

private:
    void setEntity(Entity* entity) {
        this->entity = entity;
    }
    Entity* entity{};
};


class Entity {
public:
    template <typename T, typename... Args>
      requires std::derived_from<T, Component> && std::constructible_from<T, Args...>
    T* add(Args&& ... args) {
        auto ptr = std::make_unique<T>(args...);
        ptr->setEntity(this);
        components.push_back(std::move(ptr));
        return static_cast<T*>(components.back().get());
    }

    template <std::derived_from<Component> T>
    T* get() {
        for (auto& c : components) {
            if (auto* ptr = dynamic_cast<T*>(c.get())) {
                return ptr;
            }
        }
        return nullptr;
    }

    template <std::derived_from<Component> T>
    T* remove() {
        for (size_t i = 0; i < components.size(); ++i) {
            if (auto* ptr = dynamic_cast<T*>(components[i].get())) {
                components.erase(components.begin() + i);
                return ptr;
            }
        }
        return nullptr;
    }

private:
    std::vector<std::unique_ptr<Component>> components;
};
