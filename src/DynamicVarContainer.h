#pragma once

#include <cinder/Cinder.h>
#include <cinder/Signals.h>
#include <cinder/Log.h>

#include <unordered_map>

namespace cinder {

	/**
	 * A container to store dynamic objects.
	 *
	 * A dynamic object is an object that can be created from the JSON file,
	 * using the "dynamic" entry.
	 *
	 * A container manages all objects for a given type. It is responsible for
	 * creation and deletion, to reflect the changes in the JSON file.
	 *
	 * A container must be registered with JsonBag::addDynamicVarContainer.
	 *
	 * Several containers may be registered to the JsonBag, to manage several
	 * types of objects.
	 *
	 * To link dynamic objects together, use DynamicVar.
	 */
	struct IDynamicVarContainer
	{
		virtual ~IDynamicVarContainer()  {} 

		struct TypeAndName
		{
			std::string typeName;
			std::string name;

			friend bool operator< (const TypeAndName& left, const TypeAndName& right)
			{
				return left.typeName < right.typeName ||
					(!(right.typeName < left.typeName) && left.name < right.name)
					;
			}
		};

		/// Called by JsonBag on save to write it to the JSON file.
		virtual std::vector<TypeAndName> getContentForSave() const = 0;

		/// Called by JsonBag on load to create/destroy dynamic objects.
		virtual void loadContent(const std::vector<TypeAndName> & newContent) = 0;
	};

	/**
	 * An abstract container to manage dynamic objects of type T.
	 *
	 * @see IDynamicVarContainer
	 */
	template <class T>
	struct DynamicVarContainer : IDynamicVarContainer
	{
		using Object = T;
		using ObjectRef = std::unique_ptr<Object>;

		std::vector<TypeAndName> getContentForSave() const override
		{
			std::vector<TypeAndName> result;
			result.reserve(_content.size());
			for(const auto & item : _content)
			{
				result.push_back(item.first);
			}
			return result;
		}

		void loadContent(const std::vector<TypeAndName> & newContent) override
		{
			decltype(_content) toRemove;
			using std::swap;
			swap(toRemove, _content);

			for(const auto & item : newContent)
			{
				ObjectRef value;
				const auto it = toRemove.find(item);
				if(it == toRemove.end())
				{
					// no object with that type and name
					value = createImpl(item.typeName, item.name);
					if(!value)
					{
						// the factory cannot create "typeName" objects
						CI_LOG_E( "Cannot create dynamic object for type name: " + item.typeName );
						continue;
					}

					_mappedByName.emplace(item.name, value.get());
					Created.emit(value.get(), item.typeName, item.name);
				}
				else
				{
					// reuse existing object
					value = std::move(it->second);
					toRemove.erase(it);
				}

				_content[item] = std::move(value);
			}

			for(const auto & itemToRemove : toRemove)
			{
				const auto & it = _mappedByName.find(itemToRemove.first.name);
				if(it != _mappedByName.end())
				{
					_mappedByName.erase(it);
				}
				Destroyed.emit(itemToRemove.second.get(), itemToRemove.first.typeName, itemToRemove.first.name);
			}
		}

		bool empty() const
		{
			return _content.empty();
		}

		Object* get(const std::string & name) const
		{
			const auto it = _mappedByName.find(name);
			return (it == _mappedByName.end()) ? nullptr : it->second;
		}

		template <class FuncT>
		void foreach(FuncT && func) const
		{
			for(const auto & item : _content)
			{
				func(item.second.get(), item.first.typeName, item.first.name);
			}
		}

		Object* add(const std::string & typeName, const std::string & name)
		{
			auto object = createImpl(typeName, name);
			if(!object)
			{
				// the factory cannot create "typeName" objects
				CI_LOG_E( "Cannot create dynamic object for type name: " + typeName );
				return nullptr;
			}

			const auto & objectPtr = object.get();
			_mappedByName.emplace(name, objectPtr);
			_content[{typeName, name}] = std::move(object);
			Created.emit(objectPtr, typeName, name);
			return objectPtr;
		}

		ci::signals::Signal<void(Object *, const std::string & typeName, const std::string & name)> Created;
		ci::signals::Signal<void(Object *, const std::string & typeName, const std::string & name)> Destroyed;

	protected:
		virtual ObjectRef createImpl(const std::string & typeName, const std::string & name) const = 0;

	private:
		std::map<TypeAndName, ObjectRef> _content;
		std::unordered_map<std::string, Object *> _mappedByName;
	};

	/**
	 * An implementation of IDynamicVarContainer that instantiates T.
	 */
	template <class T, class ...Param>
	struct SimpleDynamicVarContainer : DynamicVarContainer<T>
	{
		SimpleDynamicVarContainer(const Param &... p)
			: _params(p...)
		{
		}

	protected:
		ObjectRef createImpl(const std::string & typeName, const std::string & name) const override
		{
			using ParamsIndices = std::make_integer_sequence<int, sizeof...(Param)>;
			return createImplUnpack(typeName, name, ParamsIndices {});
		}

	private:
		template <int ...I>
		ObjectRef createImplUnpack(const std::string & typeName, const std::string & name, std::integer_sequence<int, I...>) const
		{
			return std::make_unique<T>(name, std::get<I>(_params)...);
		}

		std::tuple<Param...> _params;
	};

	/**
	 * An implementation of IDynamicVarContainer that takes a factory
	 * function to create sub objects of T.
	 */
	template <class T, class ...Param>
	struct FactoryDynamicVarContainer : DynamicVarContainer<T>
	{
		using Factory = std::function<ObjectRef(const std::string& typeName, const std::string& name, const Param &...)>;

		FactoryDynamicVarContainer(Factory actualFactory, const Param &... p)
			: _params(p...)
			, _factory(std::move(actualFactory))
		{
		}

	protected:
		ObjectRef createImpl(const std::string & typeName, const std::string & name) const override
		{
			using ParamsIndices = std::make_integer_sequence<int, sizeof...(Param)>;
			return createImplUnpack(typeName, name, ParamsIndices {});
		}

	private:
		template <int ...I>
		ObjectRef createImplUnpack(const std::string & typeName, const std::string & name, std::integer_sequence<int, I...>) const
		{
			return _factory(typeName, name, std::get<I>(_params)...);
		}

		std::tuple<Param...> _params;
		Factory _factory;
	};
}
