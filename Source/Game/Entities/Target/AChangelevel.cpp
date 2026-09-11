#include "AChangelevel.h"
#include "Game.h"

namespace Freeking::Entity::Target
{
	AChangelevel::AChangelevel() : BaseEntity()
	{
	}

	void AChangelevel::Initialize()
	{
	}

	void AChangelevel::Tick(double dt)
	{
		(void)dt;
	}

	bool AChangelevel::SetProperty(const EntityProperty& property)
	{
		if (property.IsKey("map"))
		{
			_mapName = property.Value();
			return true;
		}

		return false;
	}

	void AChangelevel::OnTrigger()
	{
		// Fired by relays/counters when the level goals are done.
		if (!_mapName.empty() && Game::Instance() != nullptr)
		{
			Game::Instance()->QueueMapChange(_mapName);
		}
	}
}
