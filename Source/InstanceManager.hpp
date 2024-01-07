#pragma once

#include <unordered_map>
#include <mutex>
#include <memory>
#include <sstream>


template<typename INSTANCETYPE,int FIRST_INSTANCE_ID=1000>
class InstanceManager_t
{
public:
	typedef int Instance_t;
	
public:
	Instance_t	AllocPreinitialised(std::shared_ptr<INSTANCETYPE> PreinitialisedInstance)
	{
		const auto InstanceId = AllocInstanceId();
		AllocPreinitialisedWithId( InstanceId, PreinitialisedInstance );
		return InstanceId;
	}

	void		AllocPreinitialisedWithId(Instance_t InstanceId,std::shared_ptr<INSTANCETYPE> PreinitialisedInstance)
	{
		std::scoped_lock Lock( mInstanceLock );
		auto Existing = mInstances.find(InstanceId);
		if ( Existing != std::end(mInstances) )
			throw std::runtime_error("Instance manager already using this instance id");
		if ( !PreinitialisedInstance )
			throw std::runtime_error("Tried to insert pre-allocated null instance");
		mInstances[InstanceId] = PreinitialisedInstance;
	}

	template<typename ...ARGUMENTS>
	Instance_t	Alloc(ARGUMENTS ...Args)
	{
		static_assert(!std::is_abstract<INSTANCETYPE>(), "Cannot use this function with abstract base class");
		const auto InstanceId = AllocInstanceId();
		AllocWithId( InstanceId, Args... );
		return InstanceId;
	};
	
	template<typename ...ARGUMENTS>
	void	AllocWithId(Instance_t InstanceId,ARGUMENTS ...Args)
	{
		static_assert(!std::is_abstract<INSTANCETYPE>(), "Cannot use this function with abstract base class");

		//	insert into list
		{
			std::scoped_lock Lock( mInstanceLock );
			auto Existing = mInstances.find(InstanceId);
			if ( Existing != std::end(mInstances) )
				throw std::runtime_error("Instance manager already using this instance id");
		}

		//	can happily allocate outside of the lock
		std::shared_ptr<INSTANCETYPE> NewInstance( new INSTANCETYPE( Args... ) );

		{
			if ( !NewInstance )
				throw std::runtime_error("Allocated null instance");
			std::scoped_lock Lock( mInstanceLock );
			mInstances[InstanceId] = NewInstance;
		}
	};
	
	template<typename ALLOCTYPE,typename ...ARGUMENTS>
	Instance_t	Alloc(ARGUMENTS ...Args)
	{
		auto InstanceId = AllocInstanceId();

		{
			std::scoped_lock Lock( mInstanceLock );
			auto Existing = mInstances.find(InstanceId);
			if ( Existing != std::end(mInstances) )
				throw std::runtime_error("Instance manager already using this instance id");
		}
		
		std::shared_ptr<INSTANCETYPE> NewInstance( new ALLOCTYPE( Args... ) );
		{
			if ( !NewInstance )
				throw std::runtime_error("Allocated null instance");
			std::scoped_lock Lock( mInstanceLock );
			mInstances[InstanceId] = NewInstance;
		}
		return InstanceId;
	};
	
	void		Free(Instance_t Instance)
	{
		//	gr: do the actual free outside the lock
		//		so if one thread has started a slow free, a 2nd call does nothing
		std::shared_ptr<INSTANCETYPE> pInstance;
		{
			std::scoped_lock Lock( mInstanceLock );
			pInstance = mInstances[Instance];
			mInstances.erase(Instance);
		}
		pInstance.reset();
	}
	
	std::shared_ptr<INSTANCETYPE>	GetInstance(Instance_t Instance)
	{
		std::scoped_lock Lock( mInstanceLock );
		const auto& pInstance = mInstances[Instance];
		if ( !pInstance )
		{
			std::stringstream Error;
			Error << "Instance [" << Instance << "] does not exist";
			throw std::runtime_error(Error.str());
		}
		return pInstance;
	}
	
	bool		Exists(Instance_t Instance)
	{
		std::scoped_lock Lock( mInstanceLock );
		auto InstanceIt = mInstances.find(Instance);
		if ( InstanceIt == mInstances.end() )
			return false;
		//	gr: getting cases where null is in the list... how does this happen?
		const auto& pInstance = InstanceIt->second;
		if ( !pInstance )
			return false;
		return true;
	}
	
	size_t		GetInstanceCount()	{	return mInstances.size();	}
	
	void		FreeAll()
	{
		//	we're assuming freeing these wont add more, but another thread could do...
		std::vector<Instance_t> Keys;
		{
			std::scoped_lock Lock( mInstanceLock );
			for ( auto& Entry : mInstances )
				Keys.push_back( Entry.first );
		}

		for ( auto Instance : Keys )
			Free( Instance );
	}
	
	//	gr: this should really only ever be used for debug (enumerating instances)
	//		but it should be safe to just make a copy of the info & ptr to the instances
	std::unordered_map<Instance_t,std::shared_ptr<INSTANCETYPE>> GetInstances()
	{
		std::scoped_lock Lock(mInstanceLock);
		auto InstancesCopy = mInstances;
		return InstancesCopy;
	}
	
	Instance_t	AllocInstanceId()
	{
		std::scoped_lock Lock( mInstanceLock );
		mLastInstanceId++;
		while ( true )
		{
			auto Existing = mInstances.find(mLastInstanceId);
			if ( Existing == std::end(mInstances) )
				break;
			
			//	unexpectedly, instance already exists at this key
			mLastInstanceId++;
		}
		
		//	insert into list
		auto InstanceId = mLastInstanceId;
		return InstanceId;
	}
	
private:
	std::mutex	mInstanceLock;
	Instance_t	mLastInstanceId = FIRST_INSTANCE_ID;	//	gr: start at an arbiritry number to aid debugging. (Never expect zero, one, etc)
	
	std::unordered_map<Instance_t,std::shared_ptr<INSTANCETYPE>>	mInstances;
};


