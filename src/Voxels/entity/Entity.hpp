#pragma once

#include <memory>
#include <vector>

class Entity;

class Component {
public:
    [[nodiscard]] Entity& getEntity() const {
        return entity;
    }

    explicit Component(Entity& owner) : entity{owner} {}
    Component(const Component&) = delete;
    Component(Component&&) = delete;
    Component& operator=(const Component&) = delete;
    Component& operator=(Component&&) = delete;
    virtual ~Component() = default;

private:
    Entity& entity;
};


class Entity {
public:
    template <typename T, typename... Args>
      requires std::derived_from<T, Component> && std::constructible_from<T, Entity&, Args...>
    T* add(Args&& ... args) {
        auto ptr = std::make_unique<T>(*this, std::forward<Args>(args)...);
        components.push_back(std::move(ptr));
        return static_cast<T*>(components.back().get());
    }

    template <std::derived_from<Component> T, typename Self>
    auto* get(this Self&& self) {
        using Ret = std::conditional_t<
            std::is_const_v<std::remove_reference_t<Self>>, const T, T>;
        for (auto& c : self.components) {
            if (auto* ptr = dynamic_cast<Ret*>(c.get())) {
                return ptr;
            }
        }
        return static_cast<Ret*>(nullptr);
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
