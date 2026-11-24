#nullable enable

using System;
using System.Runtime.InteropServices;
using Godot.NativeInterop;

namespace Godot.Bridge
{
    /// <summary>
    /// Samples .NET runtime metrics for the native <c>DotNetMetricSource</c>
    /// (<c>PerformanceRecorder</c> source <c>dotnet</c> and <c>Performance</c>
    /// custom monitors <c>dotnet/*</c>). One call writes all values; it does not allocate in steady state.
    /// </summary>
    internal static class RuntimeMetrics
    {
        // Number and order of values must match DotNetMetricSource::Value in modules/mono/dotnet_metric_source.h.
        private const int ValueCount = 15;

        // GC.GetGCMemoryInfo() allocates, and its values only change after a collection,
        // so it is only called again when the collection count changed.
        private static int _memoryInfoCollectionCount = -1;
        private static double _heapSizeBytes;
        private static double _fragmentedBytes;
        private static double _committedBytes;
        private static double _pauseTimePercent;

        [UnmanagedCallersOnly]
        internal static unsafe int Sample(double* values, int count)
        {
            try
            {
                if (values == null || count < ValueCount)
                    return 0;

                int gen0 = System.GC.CollectionCount(0);
                if (gen0 != _memoryInfoCollectionCount)
                {
                    _memoryInfoCollectionCount = gen0;
                    System.GCMemoryInfo info = System.GC.GetGCMemoryInfo(System.GCKind.Any);
                    _heapSizeBytes = info.HeapSizeBytes;
                    _fragmentedBytes = info.FragmentedBytes;
                    _committedBytes = info.TotalCommittedBytes;
                    _pauseTimePercent = info.PauseTimePercentage;
                }

                values[0] = System.GC.GetTotalAllocatedBytes(false);
                values[1] = System.GC.GetTotalMemory(false);
                values[2] = _heapSizeBytes;
                values[3] = _fragmentedBytes;
                values[4] = _committedBytes;
                values[5] = _pauseTimePercent;
                values[6] = System.GC.GetTotalPauseDuration().TotalMilliseconds;
                values[7] = gen0;
                values[8] = System.GC.CollectionCount(1);
                values[9] = System.GC.CollectionCount(2);
                values[10] = System.Threading.ThreadPool.ThreadCount;
                values[11] = System.Threading.ThreadPool.PendingWorkItemCount;
                values[12] = System.Threading.ThreadPool.CompletedWorkItemCount;
                values[13] = System.Threading.Monitor.LockContentionCount;
                values[14] = System.Runtime.JitInfo.GetCompiledMethodCount(false);
                return ValueCount;
            }
            catch (Exception e)
            {
                ExceptionUtils.LogException(e);
                return 0;
            }
        }
    }
}
