#nullable enable

namespace Godot
{
    public static partial class ResourceSaver
    {
        /// <summary>
        /// Saves <paramref name="resource"/> to <paramref name="path"/> like
        /// <see cref="Save(Resource, string, SaverFlags)"/> and returns whether it succeeded,
        /// with the reason in <paramref name="error"/>.
        /// If <paramref name="path"/> is empty, <see cref="Resource.ResourcePath"/> is used.
        /// Use <see cref="SaveResult(Resource, string, SaverFlags)"/> to get the saver's message as well.
        /// </summary>
        /// <example>
        /// <code>
        /// if (!ResourceSaver.TrySave(settings, "user://settings.tres", out Error error))
        /// {
        ///     GD.PushError($"Could not save the settings: {error}");
        /// }
        /// </code>
        /// </example>
        /// <param name="resource">The resource to save.</param>
        /// <param name="path">The path to save to, for example <c>user://settings.tres</c>.</param>
        /// <param name="error"><see cref="Error.Ok"/> on success, otherwise the code of the saver.</param>
        /// <returns><see langword="true"/> if the resource was saved.</returns>
        public static bool TrySave(Resource resource, string path, out Error error)
        {
            error = Save(resource, path);
            return error == Error.Ok;
        }
    }
}
