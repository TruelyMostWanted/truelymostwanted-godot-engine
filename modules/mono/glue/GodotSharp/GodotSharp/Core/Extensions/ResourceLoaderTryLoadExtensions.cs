using System.Diagnostics.CodeAnalysis;

#nullable enable

namespace Godot
{
    public static partial class ResourceLoader
    {
        /// <summary>
        /// Loads the resource at <paramref name="path"/> like <see cref="Load{T}"/>, but returns
        /// <see langword="false"/> instead of throwing or returning <see langword="null"/> when the
        /// file is missing, cannot be loaded, or is not a <typeparamref name="T"/>.
        /// The resource is the same (cached) instance that <see cref="Load{T}"/> returns.
        /// Use <see cref="LoadResult(string, string, CacheMode)"/> to get the loader's message as well.
        /// </summary>
        /// <example>
        /// <code>
        /// if (ResourceLoader.TryLoad&lt;Texture2D&gt;("res://icon.svg", out Texture2D? icon))
        /// {
        ///     GD.Print(icon.GetSize());
        /// }
        /// </code>
        /// </example>
        /// <param name="path">The path of the resource, for example <c>res://icon.svg</c>.</param>
        /// <param name="resource">The loaded resource, or <see langword="null"/> if the method returns <see langword="false"/>.</param>
        /// <typeparam name="T">The expected type. Should be <see cref="Resource"/> or a type that derives from it.</typeparam>
        /// <returns><see langword="true"/> if the resource was loaded and is a <typeparamref name="T"/>.</returns>
        public static bool TryLoad<T>(string path, [NotNullWhen(true)] out T? resource) where T : class
        {
            return TryLoad(path, out resource, out _);
        }

        /// <summary>
        /// Loads the resource at <paramref name="path"/> like <see cref="Load{T}"/>, but returns
        /// <see langword="false"/> and the reason in <paramref name="error"/> instead of throwing or
        /// returning <see langword="null"/>.
        /// <paramref name="error"/> is <see cref="Error.FileNotFound"/> for a missing file,
        /// <see cref="Error.InvalidData"/> if the loaded resource is not a <typeparamref name="T"/>,
        /// otherwise the code of the loader (see <see cref="ResourceLoadResult.Error"/>).
        /// Use <see cref="LoadResult(string, string, CacheMode)"/> to get the loader's message as well.
        /// </summary>
        /// <example>
        /// <code>
        /// if (!ResourceLoader.TryLoad&lt;Texture2D&gt;("res://icon.svg", out Texture2D? icon, out Error error))
        /// {
        ///     GD.PushError($"Could not load the icon: {error}");
        /// }
        /// </code>
        /// </example>
        /// <param name="path">The path of the resource, for example <c>res://icon.svg</c>.</param>
        /// <param name="resource">The loaded resource, or <see langword="null"/> if the method returns <see langword="false"/>.</param>
        /// <param name="error"><see cref="Error.Ok"/> on success, otherwise the reason of the failure.</param>
        /// <typeparam name="T">The expected type. Should be <see cref="Resource"/> or a type that derives from it.</typeparam>
        /// <returns><see langword="true"/> if the resource was loaded and is a <typeparamref name="T"/>.</returns>
        public static bool TryLoad<T>(string path, [NotNullWhen(true)] out T? resource, out Error error) where T : class
        {
            ResourceLoadResult result = LoadResult(path);
            if (!result.Ok)
            {
                resource = null;
                error = result.Error;
                return false;
            }
            if (result.Resource is not T typed)
            {
                resource = null;
                error = Error.InvalidData;
                return false;
            }
            resource = typed;
            error = Error.Ok;
            return true;
        }
    }
}
