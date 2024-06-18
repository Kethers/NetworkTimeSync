/*
MIT License

Copyright (c) 2022 Younes Meziane

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#include "NetworkTimeSubsystem.h"
#include "NetworkTimeSync.h"
#include "Logging/StructuredLog.h"

UNetworkTimeSubsystem* UNetworkTimeSubsystem::Get(UObject* WorldContextObject)
{
	ensureMsgf(WorldContextObject, TEXT("Cannot get network time subsystem from a null world context object."));
    if (IsValid(WorldContextObject))
    {
    	if (const UWorld* World = WorldContextObject->GetWorld())
    	{
    		if (const UGameInstance* GI = World->GetGameInstance())
    		{
    			return GI->GetSubsystem<UNetworkTimeSubsystem>();
    		}
    	}
    }
    
	return nullptr;
}

float UNetworkTimeSubsystem::GetServerWorldTime() const
{
	return GetWorld()->GetTimeSeconds() + ServerWorldTimeDelta;
}

double UNetworkTimeSubsystem::GetTimeSinceSubsystemStart() const
{
	return (FDateTime::UtcNow() - StartTimestamp).GetTotalSeconds();
}

void UNetworkTimeSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	StartTimestamp = FDateTime::UtcNow();
}

void UNetworkTimeSubsystem::OnServerWorldTimeReceived(const float ClientSendTime, const float ServerTime, bool ForceSync)
{
	float ClientReceiveTime = GetWorld()->GetTimeSeconds();
	float RoundTripTime = ClientReceiveTime - ClientSendTime;
	ensureAlwaysMsgf(RoundTripTime >= 0.0f, TEXT("RTT is negative. ClientSendTime: %f, ClientReceiveTime: %f"), ClientSendTime, ClientReceiveTime);
	// find the minimum RTT as delta to calculate theta
	if (RoundTripTime >= 0.0f && (ForceSync || RoundTripTime < ShortestRoundTripTime))
	{
		ShortestRoundTripTime = FMath::Min(RoundTripTime, ShortestRoundTripTime);
		
		// Obviously, RTT / 2 isn't representative of all networking conditions since it assumes that RTT is perfectly split 50/50.
		// Better than nothing.
		float OldDelta = ServerWorldTimeDelta;
		ServerWorldTimeDelta = ServerTime - ClientSendTime - (RoundTripTime / 2.0f);
		// FIXME and Temp: \r\n is Windows' line break and might not work on mac/linux.
		UE_LOGFMT(LogNetworkTimeSync, Log, "NTP server world time: {0} ms\r\n\t t0: {1} ms\r\n\t t1t2: {2} ms\r\n\t t3: {3}ms \r\n\t RTT: {4} ms, WorldTimeDelta: {5} ms",
			GetServerWorldTime() * 1000.0f,
			ClientSendTime * 1000.0f,
			ServerTime * 1000.0f,
			ClientReceiveTime * 1000.0f,
			RoundTripTime * 1000.0f,
			ServerWorldTimeDelta * 1000.0f);
		
		OnNetworkClockSynchronized.Broadcast(OldDelta, ServerWorldTimeDelta);
	}
}
