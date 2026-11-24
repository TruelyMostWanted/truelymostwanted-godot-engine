def can_build(env, platform):
    env.module_add_dependencies("webm", ["opus", "vorbis"])
    return True


def get_opts(platform):
    from SCons.Variables import BoolVariable

    return [
        BoolVariable("webm_vp8", "Decode VP8 video in WebM files (in addition to VP9)", True),
    ]


def configure(env):
    pass


def get_doc_classes():
    return [
        "VideoStreamWebm",
    ]


def get_doc_path():
    return "doc_classes"
